
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
    struct hstring url = {0};
    struct hstring body = {0};
    struct http_message *msg = (struct http_message *)raw_msg;
    enum http_message_type msg_type = http_message_type(msg);
    void *ret1, *ret2;

    switch (msg_type)
    {
    case HTTP_MESSAGE_REQ_LINE:
        DEBUG_PRINT("---------------------------------------------------------------\n");
        http_message_get_request_line(msg, &req_line);
        if (req_line.uri.str)
        {
            DEBUG_PRINT("req_line.method.str: %.*s\n", req_line.method.str_len, req_line.method.str);
            ret1 = memmem(req_line.method.str, req_line.method.str_len, "PUT", 3);
            DEBUG_PRINT("req_line.version.str: %.*s\n", req_line.version.str_len, req_line.version.str);
        }
        break;
    case HTTP_MESSAGE_REQ_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            ret1 = memmem(header.key.str, header.key.str_len, "key", 3);
            ret2 = memmem(header.val.str, header.val.str_len, "val", 3);
            DEBUG_PRINT("REQ header: %.*s : %.*s\n", (int)header.key.str_len, header.key.str, (int)header.val.str_len, header.val.str);
        }
        if(http_message_get_url(msg, &url) >= 0){
            DEBUG_PRINT("URL: %.*s\n", url.str_len, url.str);
        }
        break;
    case HTTP_MESSAGE_REQ_BODY:
        // http_message_get_request_raw_body(msg, &body);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        ret1 = memmem(body.str, body.str_len, "</html>", 7);
        break;
    case HTTP_MESSAGE_RES_LINE:
        http_message_get_response_line(msg, &res_line);
        ret1 = memmem(res_line.status.str, res_line.status.str_len, "OK", 2);
        DEBUG_PRINT("res_line.status.str: %.*s\n", (int)res_line.status.str_len, res_line.status.str);
        break;
    case HTTP_MESSAGE_RES_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            ret1 = memmem(header.key.str, header.key.str_len, "key", 3);
            ret2 = memmem(header.val.str, header.val.str_len, "val", 3);
            DEBUG_PRINT("RES header: %.*s : %.*s\n", (int)header.key.str_len, header.key.str, (int)header.val.str_len, header.val.str);
        }
        break;
    case HTTP_MESSAGE_RES_BODY:
        http_message_get_raw_body(msg, &body);
        DEBUG_PRINT("res raw body: %.*s\n", body.str_len, body.str);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        ret1 = memmem(body.str, body.str_len, "</html>", 7);
        DEBUG_PRINT("res unzip body: %.*s\n", body.str_len, body.str);
        DEBUG_PRINT("---------------------------------------------------------------\n");
        break;

        // to do: check payload
    default:
        break;
    }
    return;
}

static on_msg_cb_func *g_entry_fun = &http_decoder_perf_entry;

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

extern "C" void http_decoder_plug_set_entry_fuc(on_msg_cb_func *entry_fun)
{
    g_entry_fun = entry_fun;
}