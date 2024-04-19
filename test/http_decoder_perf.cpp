
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "../include/http_decoder.h"

#ifdef __cplusplus
extern "C"
{

#include "cJSON.h"
#include "http_decoder_gtest.h"
#include "stellar/utils.h"
#include "stellar/stellar.h"
#include "stellar/session_exdata.h"
#include "stellar/session_mq.h"
#include "md5.h"
}
#endif

#define MAX_KEY_STR_LEN 2048

static int g_result_count = 0;
static int g_header_count = 1;
static int g_exdata_idx = 0;
static int g_topic_id = 0;

extern "C" int http_decoder_perf_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    struct http_request_line req_line = {0};
    struct http_response_line res_line = {0};
    struct http_header header = {0};
    struct hstring body = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);
    void *ret1, *ret2;

    switch (msg_type)
    {
    case HTTP_MESSAGE_REQ_LINE:
        http_message_get_request_line(msg, &req_line);
        if (req_line.uri.str)
        {
            ret1 = memmem(req_line.uri.str, req_line.uri.str_len, "index", 5);
        }
        break;
    case HTTP_MESSAGE_REQ_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            ret1 = memmem(header.key.str, header.key.str_len, "key", 3);
            ret2 = memmem(header.val.str, header.val.str_len, "val", 3);
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
        break;
    case HTTP_MESSAGE_RES_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            ret1 = memmem(header.key.str, header.key.str_len, "key", 3);
            ret2 = memmem(header.val.str, header.val.str_len, "val", 3);
        }
        break;
    case HTTP_MESSAGE_RES_BODY:
        // http_message_get_response_raw_body(msg, &body);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        ret1 = memmem(body.str, body.str_len, "</html>", 7);
        break;

        // to do: check payload
    default:
        break;
    }

    if(ret1 && ret2){
        return 1;
    }
    return 0;
}

static int (*g_entry_fun)(struct session *sess, int topic_id, const void *data, void *cb_arg) = &http_decoder_perf_entry;

static void http_decoder_test_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
    return;
}

extern "C" void *http_decoder_perf_init(struct stellar *st)
{
    g_exdata_idx =
        stellar_session_get_ex_new_index(st, "HTTP_DECODER_REQ_TEST",
                                         http_decoder_test_exdata_free,
                                         NULL);
    if (g_exdata_idx < 0)
    {
        printf("[%s:%d]: can't get http_decoder exdata index !!!\n",
               __FUNCTION__, __LINE__);
        exit(-1);
    }

    g_topic_id = session_mq_get_topic_id(st, "HTTP_DECODER_MESSAGE");
    if (g_topic_id < 0)
    {
        printf("[%s:%d]: can't get http_decoder topic id !!!\n",
               __FUNCTION__, __LINE__);
        exit(-1);
    }

    session_mq_subscribe_topic(st, g_topic_id, g_entry_fun, NULL);
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

extern "C" void http_decoder_plug_set_entry_fuc(HTTP_DECODER_PLUG_ENTRY_FUN_T entry_fun)
{
    g_entry_fun = entry_fun;
}