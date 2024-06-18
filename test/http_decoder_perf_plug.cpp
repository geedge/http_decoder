#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#include "http_decoder_inc.h"
#include "cJSON.h"
}
#endif

#define MAX_KEY_STR_LEN 2048

static int g_result_count = 0;
static int g_header_count = 1;
static int g_exdata_idx = -1;
static int g_topic_id = -1;
static int g_plugin_id = -1;

#define DEBUG_PRINT(fmt, ...)  ///printf(fmt, ##__VA_ARGS__)

extern "C" void http_decoder_perf_entry(struct session *sess, int topic_id, const void *raw_msg, void *per_session_ctx, void *plugin_env)
// extern "C" int http_decoder_perf_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    struct http_request_line req_line = {0};
    struct http_response_line res_line = {0};
    struct http_header header = {0};
    hstring url = {0};
    hstring body = {0};
    struct http_message *msg = (struct http_message *)raw_msg;
    enum http_message_type msg_type = http_message_type_get(msg);
    void *ret1, *ret2;

    switch (msg_type)
    {
    case HTTP_MESSAGE_REQ_LINE:
        DEBUG_PRINT("---------------------------------------------------------------\n");
        http_message_get_request_line(msg, &req_line);
        if (req_line.uri.iov_base)
        {
            DEBUG_PRINT("req_line.method.iov_base: %.*s\n", req_line.method.iov_len, req_line.method.iov_base);
            ret1 = memmem(req_line.method.iov_base, req_line.method.iov_len, "PUT", 3);
            DEBUG_PRINT("req_line.version.iov_base: %.*s\n", req_line.version.iov_len, req_line.version.iov_base);
        }
        break;
    case HTTP_MESSAGE_REQ_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            ret1 = memmem(header.key.iov_base, header.key.iov_len, "key", 3);
            ret2 = memmem(header.val.iov_base, header.val.iov_len, "val", 3);
            DEBUG_PRINT("REQ header: %.*s : %.*s\n", (int)header.key.iov_len, header.key.iov_base, (int)header.val.iov_len, header.val.iov_base);
        }
        http_message_get_url(msg, &url);
        if(url.iov_base && url.iov_len){
            DEBUG_PRINT("URL: %.*s\n", url.iov_len, url.iov_base);
        }
        break;
    case HTTP_MESSAGE_REQ_BODY:
        // http_message_get_request_raw_body(msg, &body);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        ret1 = memmem(body.iov_base, body.iov_len, "</html>", 7);
        break;
    case HTTP_MESSAGE_RES_LINE:
        http_message_get_response_line(msg, &res_line);
        ret1 = memmem(res_line.status.iov_base, res_line.status.iov_len, "OK", 2);
        DEBUG_PRINT("res_line.status.iov_base: %.*s\n", (int)res_line.status.iov_len, res_line.status.iov_base);
        break;
    case HTTP_MESSAGE_RES_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            ret1 = memmem(header.key.iov_base, header.key.iov_len, "key", 3);
            ret2 = memmem(header.val.iov_base, header.val.iov_len, "val", 3);
            DEBUG_PRINT("RES header: %.*s : %.*s\n", (int)header.key.iov_len, header.key.iov_base, (int)header.val.iov_len, header.val.iov_base);
        }
        break;
    case HTTP_MESSAGE_RES_BODY:
        http_message_get_raw_body(msg, &body);
        DEBUG_PRINT("res raw body: %.*s\n", body.iov_len, body.iov_base);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        ret1 = memmem(body.iov_base, body.iov_len, "</html>", 7);
        DEBUG_PRINT("res unzip body: %.*s\n", body.iov_len, body.iov_base);
        DEBUG_PRINT("---------------------------------------------------------------\n");
        break;

        // to do: check payload
    default:
        break;
    }
    return;
}

static  on_session_msg_cb_func*g_entry_fun = &http_decoder_perf_entry;

static void http_decoder_test_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
    return;
}

extern "C" void *http_decoder_perf_init(struct stellar *st)
{
    g_plugin_id = stellar_session_plugin_register(st, NULL, NULL, NULL);
    g_exdata_idx = stellar_session_exdata_new_index(st, "HTTP_DECODER_REQ_TEST",
                                         http_decoder_test_exdata_free,
                                            NULL);
    if (g_exdata_idx < 0)
    {
        printf("[%s:%d]: can't get http_decoder exdata index !!!\n", __FUNCTION__, __LINE__);
        exit(-1);
    }

    g_topic_id = stellar_session_mq_get_topic_id(st, "HTTP_DECODER_MESSAGE");
    if (g_topic_id < 0)
    {
        printf("[%s:%d]: can't get http_decoder topic id !!!\n", __FUNCTION__, __LINE__);
        exit(-1);
    }

    stellar_session_mq_subscribe(st, g_topic_id, g_entry_fun, g_plugin_id);
    // printf("http_decoder_test_init OK!\n");

    return NULL;
}

extern "C" void http_decoder_perf_exit(void *test_ctx)
{
    if (test_ctx != NULL)
    {
        FREE(test_ctx);
    }
    printf("http_decoder_perf plug exit OK!\n");
}
