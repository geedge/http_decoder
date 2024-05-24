#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "http_decoder_inc.h"
#include "llhttp.h"

struct http_decoder_half_data
{
    struct http_decoder_table *table;

    int major_version;
    int minor_version;
    int status_code;

    enum http_content_encoding content_encoding;
    struct http_content_decompress *decompress;
    char *ref_decompress_body;
    size_t decompress_body_len;

    int joint_url_complete;
    struct hstring joint_url; // http://<host>[:<port>]/<path>?<searchpart>
    long long transaction_index;
};

struct http_decoder_half
{
    llhttp_t parser;
    llhttp_settings_t settings;
    enum llhttp_errno error;
    int decompress_switch;

    // uint8_t is_request_flow;
    enum http_event event;
    http_event_cb *http_ev_cb;
    struct http_event_context *http_ev_ctx;

    struct http_decoder_half_data *ref_data;

    long long trans_counter;
    long long err_counter;
    long long transaction_seq;

    const char *data;
    int data_len;
};

// #define HTTP_DECODER_DEBUG
#ifdef HTTP_DECODER_DEBUG
static void printf_debug_info(const char *desc, const char *at, size_t length)
{
    if (at)
    {
        char *temp = safe_dup(at, length);
        printf("HTTP PARSER STAGE: %s: %s\n", desc, temp);
        FREE(temp);
    }
    else
    {
        printf("HTTP PARSER STAGE: %s\n", desc);
    }
}
#else
#define printf_debug_info(desc, at, length) 
#endif

static void
http_decoder_half_data_decompress(struct http_decoder_half_data *data)
{
    assert(data);

    if (data->content_encoding == HTTP_CONTENT_ENCODING_NONE)
    {
        return;
    }

    struct hstring raw_body = {0};
    http_decoder_table_get_body(data->table, &raw_body);
    if (raw_body.str == NULL || raw_body.str_len == 0)
    {
        return;
    }

    if (NULL == data->decompress)
    {
        data->decompress = http_content_decompress_create(data->content_encoding);
    }

    assert(data->decompress);
    if (http_content_decompress_write(data->decompress, raw_body.str,
                                      raw_body.str_len,
                                      &data->ref_decompress_body,
                                      &data->decompress_body_len) == -1)
    {
        // log error
        http_content_decompress_destroy(data->decompress);
        data->decompress = NULL;
    }
}

/* Possible return values 0, -1, `HPE_PAUSED` */
static int on_message_begin(llhttp_t *http)
{
    printf_debug_info("on_message_begin", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->parser.type == HTTP_REQUEST)
    {
        half->event = HTTP_EVENT_REQ_INIT;
    }
    else
    {
        half->event = HTTP_EVENT_RES_INIT;
    }

    half->ref_data = NULL;

    assert(half->http_ev_cb != NULL);
    half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx); // http_event_handler()

    half->trans_counter++;
    half->ref_data->transaction_index = half->transaction_seq++;
    return 0;
}

static int on_message_complete(llhttp_t *http)
{
    printf_debug_info("on_message_complete", NULL, 0);

    struct http_decoder_half *half = container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (half->parser.type == HTTP_REQUEST)
    {
        if (half->event == HTTP_EVENT_REQ_BODY_DATA)
        {
            half->event = HTTP_EVENT_REQ_BODY_END;
            half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
        }
    }
    else
    {
        if (half->event == HTTP_EVENT_RES_BODY_DATA)
        {
            half->event = HTTP_EVENT_RES_BODY_END;
            half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
        }
    }

    // trigger req_end/res_end
    if (half->parser.type == HTTP_REQUEST)
    {
        half->event = HTTP_EVENT_REQ_END;
        half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
    }
    else
    {
        half->event = HTTP_EVENT_RES_END;
        half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
    }

    return 0;
}

static int on_reset(llhttp_t *http)
{
    printf_debug_info("on_reset", NULL, 0);

    return 0;
}

static inline int is_line_crlf(struct http_decoder_half *half)
{
    const char *chr_r = (char *)memrchr(half->data, '\r', half->data_len);
    const char *chr_n = (char *)memrchr(half->data, '\n', half->data_len);
    if (chr_r && chr_n && (chr_r + 1 == chr_n))
    {
        return 1;
    }
    return 0;
}

static int on_method(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_method", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_METHOD,
                             at, length);
    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_method_complete(llhttp_t *http)
{
    printf_debug_info("on_method_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (is_line_crlf(half) == 0)
    {
        http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_METHOD);
    }

    http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_METHOD);

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_uri(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_uri", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_URI,
                             at, length);
    return 0;
}

static void http_decoder_cached_portion_url(struct http_decoder_half *half, const struct hstring *uri_result)
{
    struct http_decoder_half_data *ref_data = half->ref_data;
    int uri_skip_len = 0;

    if ((uri_result->str_len) > 7 && (strncasecmp("http://", uri_result->str, 7) == 0)) // absolute URI
    {
        uri_skip_len = strlen("http://");
        ref_data->joint_url_complete = 1;
    }
    else
    {
        ref_data->joint_url_complete = 0;
    }

    ref_data->joint_url.str_len = uri_result->str_len - uri_skip_len;
    ref_data->joint_url.str = MEMPOOL_CALLOC(half->http_ev_ctx->ref_mempool, char, ref_data->joint_url.str_len);
    memcpy(ref_data->joint_url.str, uri_result->str + uri_skip_len, ref_data->joint_url.str_len);
}

/* Information-only callbacks, return value is ignored */
static int on_uri_complete(llhttp_t *http)
{
    printf_debug_info("on_uri_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (is_line_crlf(half) == 0)
    {
        http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_URI);
    }

    http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_URI);

    struct hstring uri_result = {};
    http_decoder_table_get_uri(half->ref_data->table, &uri_result);
    assert(uri_result.str);
    http_decoder_cached_portion_url(half, &uri_result);

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_version(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_version", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_VERSION,
                             at, length);
    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_version_complete(llhttp_t *http)
{
    printf_debug_info("on_version_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (is_line_crlf(half) == 0)
    {
        http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_VERSION);
    }

    http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_VERSION);

    half->ref_data->major_version = llhttp_get_http_major(&half->parser);
    half->ref_data->minor_version = llhttp_get_http_minor(&half->parser);

    if (half->parser.type == HTTP_REQUEST)
    {
        half->event = HTTP_EVENT_REQ_LINE;
        if (half->http_ev_cb) // http_event_handler()
        {
            half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
        }
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_status(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_status", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_STATUS,
                             at, length);
    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_status_complete(llhttp_t *http)
{
    printf_debug_info("on_status_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (is_line_crlf(half) == 0)
    {
        http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_STATUS);
    }

    http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_STATUS);
    half->ref_data->status_code = llhttp_get_status_code(&half->parser);

    if (half->parser.type == HTTP_RESPONSE)
    {
        half->event = HTTP_EVENT_RES_LINE;
        if (half->http_ev_cb != NULL) // http_event_handler()
        {
            half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
        }
    }

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_header_field(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_header_field", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_HDRKEY,
                             at, length);
    return 0;
}

/* Information-only callbacks, return value is ignored */
static int on_header_field_complete(llhttp_t *http)
{
    printf_debug_info("on_header_field_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_HDRKEY);

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_header_value(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_header_value", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_HDRVAL,
                             at, length);
    return 0;
}

#define MAX_ENCODING_STR_LEN 8
/* Information-only callbacks, return value is ignored */
static int on_header_value_complete(llhttp_t *http)
{
    printf_debug_info("on_header_value_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_HDRKEY) ==
        STRING_STATE_CACHE)
    {
        http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_HDRKEY);
    }

    http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_HDRVAL);

    if (half->ref_data->content_encoding == HTTP_CONTENT_ENCODING_NONE)
    {
        struct http_header http_hdr = {0};
        struct hstring key = {.str = (char *)"Content-Encoding", .str_len = 16};

        if (http_decoder_table_get_header(half->ref_data->table, &key, &http_hdr) == 0)
        {
            char encoding_str[MAX_ENCODING_STR_LEN + 1] = {0};
            size_t str_len = http_hdr.val.str_len;
            if (str_len > MAX_ENCODING_STR_LEN)
            {
                str_len = MAX_ENCODING_STR_LEN;
            }
            memcpy(encoding_str, http_hdr.val.str, str_len);
            half->ref_data->content_encoding = http_content_encoding_str2int(encoding_str);
        }
    }

    http_decoder_get_host_feed_url(half);

    return 0;
}

/* When on_chunk_header is called, the current chunk length is stored
 * in parser->content_length.
 * Possible return values 0, -1, `HPE_PAUSED`
 */
static int on_chunk_header(llhttp_t *http)
{
    printf_debug_info("on_chunk_header", NULL, 0);

    return 0;
}

/* When on_chunk_header is called, the current chunk length is stored
 * in parser->content_length.
 * Possible return values 0, -1, `HPE_PAUSED`
 */
static int on_chunk_header_complete(llhttp_t *http)
{
    printf_debug_info("on_chunk_header_complete", NULL, 0);

    return 0;
}

/* Possible return values:
 *  0  - Proceed normally
 *  1  - Assume that request/response has no body, and proceed to parsing the next message
 *  2  - Assume absence of body (as above) and make `llhttp_execute()` return `HPE_PAUSED_UPGRADE`
 * -1  - Error `HPE_PAUSED`
 */
static int on_headers_complete(llhttp_t *http)
{
    printf_debug_info("on_headers_complete", NULL, 0);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);
    assert(half->ref_data);

    http_decoder_table_set_header_complete(half->ref_data->table);

    if (half->parser.type == HTTP_REQUEST)
    {
        half->event = HTTP_EVENT_REQ_HDR_END;
    }
    else
    {
        half->event = HTTP_EVENT_RES_HDR_END;
    }
    half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);  // http_event_handler()

    return 0;
}

/* Possible return values 0, -1, HPE_USER */
static int on_body(llhttp_t *http, const char *at, size_t length)
{
    printf_debug_info("on_body", at, length);

    struct http_decoder_half *half =
        container_of(http, struct http_decoder_half, parser);
    assert(half);

    // trigger body_begin event
    if (half->parser.type == HTTP_REQUEST)
    {
        if (half->event == HTTP_EVENT_REQ_HDR_END)
        {
            half->event = HTTP_EVENT_REQ_BODY_BEGIN;
            half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx); // http_event_handler()
        }
    }
    else
    {
        if (half->event == HTTP_EVENT_RES_HDR_END)
        {
            half->event = HTTP_EVENT_RES_BODY_BEGIN;
            half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
        }
    }

    if (half->ref_data != NULL)
    {
        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_BODY) ==
            STRING_STATE_COMMIT)
        {
            http_decoder_table_reset(half->ref_data->table, HTTP_ITEM_BODY);
        }

        http_decoder_table_refer(half->ref_data->table, HTTP_ITEM_BODY,
                                 at, length);
        http_decoder_table_commit(half->ref_data->table, HTTP_ITEM_BODY);
    }

    if (1 == half->decompress_switch &&
        half->ref_data->content_encoding != HTTP_CONTENT_ENCODING_NONE)
    {
        http_decoder_half_data_decompress(half->ref_data);
    }

    if (half->parser.type == HTTP_REQUEST)
    {
        half->event = HTTP_EVENT_REQ_BODY_DATA;
        half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx); // http_event_handler()
    }
    else
    {
        half->event = HTTP_EVENT_RES_BODY_DATA;
        half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);
    }

    return 0;
}

static void http_decoder_half_init(struct http_decoder_half *half,
                       http_event_cb *http_ev_cb, enum llhttp_type type)
{
    llhttp_settings_init(&half->settings);
    llhttp_init(&half->parser, type, &half->settings);

    // half->is_request_flow = (type == HTTP_REQUEST) ? 1 : 0;
    half->settings.on_message_begin = on_message_begin;
    half->settings.on_message_complete = on_message_complete;
    half->settings.on_reset = on_reset;

    half->settings.on_url = on_uri;
    half->settings.on_url_complete = on_uri_complete;

    half->settings.on_status = on_status;
    half->settings.on_status_complete = on_status_complete;

    half->settings.on_method = on_method;
    half->settings.on_method_complete = on_method_complete;

    half->settings.on_version = on_version;
    half->settings.on_version_complete = on_version_complete;

    half->settings.on_header_field = on_header_field;
    half->settings.on_header_field_complete = on_header_field_complete;

    half->settings.on_header_value = on_header_value;
    half->settings.on_header_value_complete = on_header_value_complete;

    half->settings.on_chunk_header = on_chunk_header;
    half->settings.on_chunk_complete = on_chunk_header_complete;

    half->settings.on_headers_complete = on_headers_complete;
    half->settings.on_body = on_body;

    half->error = HPE_OK;
    half->http_ev_cb = http_ev_cb;  // http_event_handler()
    half->ref_data = NULL;
}

struct http_decoder_half *
http_decoder_half_new(nmx_pool_t *mempool, http_event_cb *ev_cb, enum llhttp_type http_type,
                      int decompress_switch)
{
    struct http_decoder_half *half = MEMPOOL_CALLOC(mempool, struct http_decoder_half, 1);
    assert(half);

    half->decompress_switch = decompress_switch;
    half->http_ev_ctx = MEMPOOL_CALLOC(mempool, struct http_event_context, 1);
    http_decoder_half_init(half, ev_cb, http_type);

    return half;
}

void http_decoder_half_free(nmx_pool_t *mempool, struct http_decoder_half *half)
{
    if (NULL == half)
    {
        return;
    }

    if (half->http_ev_ctx != NULL)
    {
        MEMPOOL_FREE(mempool, half->http_ev_ctx);
        half->http_ev_ctx = NULL;
    }

    MEMPOOL_FREE(mempool, half);
}

void http_decoder_half_reinit(struct http_decoder_half *half, int topic_id,
                              struct http_decoder_result_queue *queue,
                              nmx_pool_t *mempool, struct session *sess)
{
    assert(half != NULL);
    if (half->ref_data != NULL)
    {
        http_decoder_table_reinit(half->ref_data->table);
    }

    half->http_ev_ctx->topic_id = topic_id;
    half->http_ev_ctx->ref_mempool = mempool;
    half->http_ev_ctx->ref_session = sess;
    half->http_ev_ctx->ref_queue = queue;
}

static void publish_message_for_parsed_header(struct http_decoder_half *half)
{
    if (0 == http_decoder_table_has_parsed_header(half->ref_data->table))
    {
        return;
    }
    if (half->parser.type == HTTP_REQUEST){
        half->event = HTTP_EVENT_REQ_HDR;
    }else{
        half->event = HTTP_EVENT_RES_HDR;
    }
    half->http_ev_cb(half->event, &half->ref_data, half->http_ev_ctx);  // http_event_handler();
    return;
}

int http_decoder_half_parse(struct http_decoder_half *half, const char *data,
                            size_t data_len)
{
    if (NULL == half || NULL == data || 0 == data_len)
    {
        return -1;
    }

    half->data = (const char *)data;
    half->data_len = data_len;
    half->error = llhttp_execute(&half->parser, data, data_len);

    int ret = 0;
    enum llhttp_type type = HTTP_BOTH;
    struct http_decoder_half_data *half_data = NULL;

    switch (half->error)
    {
    case HPE_OK:
        break;
    case HPE_PAUSED:
        llhttp_resume(&half->parser);
        break;
    case HPE_PAUSED_UPGRADE:
        llhttp_resume_after_upgrade(&half->parser);
        ret = 0;
        break;
    default:
        type = (enum llhttp_type)half->parser.type;
        llhttp_init(&half->parser, type, &half->settings);
        ret = -1;
        break;
    }

    if (ret < 0)
    {
        // fprintf(stdout,
        //         "llhttp_execute parse error: %s err_reason:%s\n",
        //         llhttp_errno_name(half->error), half->parser.reason);
        return half->error;
    }

    if (half->ref_data != NULL)
    {
        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_URI) == STRING_STATE_REFER)
        {
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_URI);
        }

        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_STATUS) == STRING_STATE_REFER)
        {
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_STATUS);
        }

        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_METHOD) == STRING_STATE_REFER)
        {
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_METHOD);
        }

        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_VERSION) == STRING_STATE_REFER)
        {
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_VERSION);
        }

        if (http_decoder_table_header_complete(half->ref_data->table))
        {
            http_decoder_table_reset_header_complete(half->ref_data->table);
        }
        else 
        {
            //if headers are not completed with EOF \r\n\r\n, push the parsed headers so far
            publish_message_for_parsed_header(half);
        }

        enum string_state hdr_key_state =
            http_decoder_table_state(half->ref_data->table, HTTP_ITEM_HDRKEY);
        enum string_state hdr_val_state =
            http_decoder_table_state(half->ref_data->table, HTTP_ITEM_HDRVAL);

        /*  Truncated in http header key
            For example http header k-v => User-Agent: Chrome
            case1:
                packet1: User-     hdr_key_state == STRING_STATE_REFER
                packet2: Agent: Chrome

            case2:
                packet1: User-Agent: hdr_key_state == STRING_STATE_COMMIT
                                     hdr_val_state == STRING_STATE_INIT
                packet2: Chrome
        */
        if (hdr_key_state == STRING_STATE_REFER ||
            (hdr_key_state == STRING_STATE_COMMIT && hdr_val_state == STRING_STATE_INIT))
        {
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_HDRKEY);
        }

        /* Truncated in http header value
           For example http header k-v => User-Agent: Chrome
           packet1: User-Agent: Ch   hdr_key_state == STRING_STATE_COMMIT
                                     hdr_val_state == STRING_STATE_REFER

           packet2: rome
        */
        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_HDRVAL) == STRING_STATE_REFER)
        {
            /* Header key should have been committed
               If it's not cached, cache it for next packet to use
            */
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_HDRKEY);
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_HDRVAL);
        }

        if (http_decoder_table_state(half->ref_data->table, HTTP_ITEM_BODY) == STRING_STATE_REFER)
        {
            http_decoder_table_cache(half->ref_data->table, HTTP_ITEM_BODY);
        }
    }

    return 0;
}

long long http_decoder_half_trans_count(struct http_decoder_half *half)
{
    if (NULL == half)
    {
        return 0;
    }

    long long trans_cnt = half->trans_counter;
    half->trans_counter = 0;

    return trans_cnt;
}

struct http_decoder_half_data *
http_decoder_half_data_new(nmx_pool_t *mempool)
{
    struct http_decoder_half_data *data =
        MEMPOOL_CALLOC(mempool, struct http_decoder_half_data, 1);
    assert(data);

    data->table = http_decoder_table_new(mempool);
    assert(data->table);

    data->major_version = -1;
    data->minor_version = -1;
    data->status_code = -1;

    data->content_encoding = HTTP_CONTENT_ENCODING_NONE;
    data->ref_decompress_body = NULL;
    data->decompress_body_len = 0;

    return data;
}

void http_decoder_half_data_free(nmx_pool_t *mempool,
                                 struct http_decoder_half_data *data)
{
    if (NULL == data)
    {
        return;
    }

    if (data->table != NULL)
    {
        http_decoder_table_free(data->table);
        data->table = NULL;
    }

    if (data->decompress != NULL)
    {
        http_content_decompress_destroy(data->decompress);
        data->decompress = NULL;
    }

    if (data->joint_url.str)
    {
        MEMPOOL_FREE(mempool, data->joint_url.str);
        data->joint_url.str = NULL;
        data->joint_url_complete = 0;
    }

    MEMPOOL_FREE(mempool, data);
}

int http_decoder_half_data_get_request_line(struct http_decoder_half_data *data,
                                            struct http_request_line *line)
{
    http_decoder_table_get_method(data->table, &line->method);
    http_decoder_table_get_uri(data->table, &line->uri);
    http_decoder_table_get_version(data->table, &line->version);

    line->major_version = data->major_version;
    line->minor_version = data->minor_version;

    return 0;
}

int http_decoder_half_data_get_response_line(struct http_decoder_half_data *data,
                                             struct http_response_line *line)
{
    http_decoder_table_get_version(data->table, &line->version);
    http_decoder_table_get_status(data->table, &line->status);

    line->major_version = data->major_version;
    line->minor_version = data->minor_version;
    line->status_code = data->status_code;

    return 0;
}

int http_decoder_half_data_get_header(const struct http_decoder_half_data *data,
                                      const struct hstring *key,
                                      struct http_header *hdr_result)
{
    return http_decoder_table_get_header(data->table, key, hdr_result);
}

int http_decoder_half_data_iter_header(struct http_decoder_half_data *data,
                                       struct http_header *header)
{
    return http_decoder_table_iter_header((struct http_decoder_table *)data->table, header);
}

int http_decoder_half_data_reset_header_iter(struct http_decoder_half_data *req_data)
{
    if (NULL == req_data)
    {
        return -1;
    }
    return http_decoder_table_reset_header_iter(req_data->table);
}

int http_decoder_half_data_has_parsed_header(struct http_decoder_half_data *data)
{
    if (NULL == data)
    {
        return 0;
    }
    return http_decoder_table_has_parsed_header(data->table);
}

int http_decoder_half_data_get_raw_body(const struct http_decoder_half_data *data,
                                        struct hstring *body)
{
    if (NULL == data || NULL == body)
    {
        return -1;
    }
    return http_decoder_table_get_body(data->table, body);
}

int http_decoder_half_data_get_decompress_body(const struct http_decoder_half_data *data,
                                               struct hstring *body)
{
    if (HTTP_CONTENT_ENCODING_NONE == data->content_encoding)
    {
        return http_decoder_table_get_body(data->table, body);
    }

    body->str = data->ref_decompress_body;
    body->str_len = data->decompress_body_len;
    return 0;
}

void http_decoder_half_data_dump(struct http_decoder_half *half)
{
    if (NULL == half || NULL == half->ref_data)
    {
        return;
    }

    http_decoder_table_dump(half->ref_data->table);
}

static void using_session_addr_as_host(struct session *ref_session,
                                       struct http_header *host_result, nmx_pool_t *mempool)
{
    const struct session_addr *ssaddr;
    enum session_addr_type ssaddr_type;
    ssaddr = session_get0_addr(ref_session, &ssaddr_type);
    if (!ssaddr)
    {
        assert(0);
    }

    char ip_string_buf[INET6_ADDRSTRLEN];
    if (SESSION_ADDR_TYPE_IPV4_TCP == ssaddr_type || SESSION_ADDR_TYPE_IPV4_UDP == ssaddr_type)
    {
        host_result->val.str = MEMPOOL_CALLOC(mempool, char, (INET_ADDRSTRLEN + 7) /* "ip:port" max length */);
        inet_ntop(AF_INET, &ssaddr->ipv4.daddr, ip_string_buf, INET_ADDRSTRLEN);
        sprintf(host_result->val.str, "%s:%u", ip_string_buf, ntohs(ssaddr->ipv4.dport));
        host_result->val.str_len = strlen(host_result->val.str);
    }
    else if (SESSION_ADDR_TYPE_IPV6_TCP == ssaddr_type || SESSION_ADDR_TYPE_IPV6_UDP == ssaddr_type)
    {
        host_result->val.str = MEMPOOL_CALLOC(mempool, char, (INET6_ADDRSTRLEN + 7) /* "ip:port" max length */);
        inet_ntop(AF_INET6, &ssaddr->ipv6.daddr, ip_string_buf, INET6_ADDRSTRLEN);
        sprintf(host_result->val.str, "%s:%u", ip_string_buf, ntohs(ssaddr->ipv6.dport));
        host_result->val.str_len = strlen(host_result->val.str);
    }
    else
    {
        assert(0);
    }
}

void http_decoder_join_url(struct http_decoder_half_data *hfdata, nmx_pool_t *mempool, const struct http_header *host_hdr)
{
    int append_slash_len = 0;
    if ('/' != hfdata->joint_url.str[0])
    {
        append_slash_len = 1;
    }
    int url_cache_str_len = host_hdr->val.str_len + hfdata->joint_url.str_len + append_slash_len;
    char *url_cache_str = MEMPOOL_CALLOC(mempool, char, url_cache_str_len);

    char *ptr = url_cache_str;
    memcpy(ptr, host_hdr->val.str, host_hdr->val.str_len);
    ptr += host_hdr->val.str_len;
    if (append_slash_len)
    {
        *ptr = '/';
        ptr++;
    }
    memcpy(ptr, hfdata->joint_url.str, hfdata->joint_url.str_len);

    MEMPOOL_FREE(mempool, hfdata->joint_url.str); // free the cached uri buffer
    hfdata->joint_url.str = url_cache_str;
    hfdata->joint_url.str_len = url_cache_str_len;

    hfdata->joint_url_complete = 1;
}

int http_decoder_join_url_finally(struct http_event_context *ev_ctx,
                                  struct http_decoder_half_data *hfdata,
                                  nmx_pool_t *mempool)
{
    struct http_header addr_as_host = {};

    if (hfdata->joint_url_complete)
    {
        return 0;
    }

    using_session_addr_as_host(ev_ctx->ref_session, &addr_as_host, mempool);
    http_decoder_join_url(hfdata, mempool, &addr_as_host);
    MEMPOOL_FREE(mempool, addr_as_host.val.str); // free session addr to host buffer
    return 1;
}

void http_decoder_get_host_feed_url(struct http_decoder_half *half)
{
    struct http_header host_result = {};
    struct hstring host_key = {(char *)"Host", 4};
    const char *host_refer_str = NULL;
    int host_refer_len = 0;

    if (half->ref_data->joint_url_complete)
    {
        return;
    }

    int host_header_cnt = http_decoder_half_data_get_header(half->ref_data, &host_key,
                                                            &host_result);
    if (host_header_cnt < 0)
    {
        return;
    }

    http_decoder_join_url(half->ref_data, half->http_ev_ctx->ref_mempool, &host_result);
}

int http_half_data_get_url(struct http_decoder_half_data *res_data, struct hstring *url)
{
    if (0 == res_data->joint_url_complete)
    {
        return -1;
    }
    url->str = res_data->joint_url.str;
    url->str_len = res_data->joint_url.str_len;
    return 0;
}

int http_half_data_get_transaction_seq(struct http_decoder_half_data *hf_data)
{
    return hf_data->transaction_index;
}

void http_half_data_update_commit_index(struct http_decoder_half_data * half_data)
{
    http_decoder_table_update_commit_index(half_data->table);
}