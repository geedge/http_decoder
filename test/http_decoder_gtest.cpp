/*
**********************************************************************************************
*	File: http_decoder_gtest.cpp
*   Description:
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2023-12-15
*   Copyright: (c) Since 2023 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

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
    int commit_test_result_json(cJSON *node, const char *name);
    extern void http_decoder_test_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env);
    static on_msg_cb_func *g_entry_fun = http_decoder_test_entry;
}
#endif

#define MAX_KEY_STR_LEN 2048

enum http_transaction_type
{
    HTTP_TRANSACTION_REQ = 0,
    HTTP_TRANSACTION_RES,
    HTTP_TRANSACTION_SESSION, // global session info
    HTTP_TRANSACTION_MAX
};

struct gtest_plug_exdata_t
{
    cJSON *result_jnode[HTTP_TRANSACTION_MAX];
    MD5_CTX *md5ctx[HTTP_TRANSACTION_MAX];
};

static int g_result_count = 0;
static int g_header_count = 1;
static int g_http_gtest_plugin_id = -1;
static int g_exdata_idx = -1;
static int g_topic_id = -1;



#if 0
void output_http_req_line(struct http_request_line *req_line)
{
    char tmp_str[MAX_KEY_STR_LEN] = {0};
    memcpy(tmp_str, req_line->method.str, req_line->method.str_len);
    printf("req_method:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    memcpy(tmp_str, req_line->uri.str, req_line->uri.str_len);
    printf("req_uri:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    memcpy(tmp_str, req_line->version.str, req_line->version.str_len);
    printf("req_version:%s\n", tmp_str);
}

void output_http_res_line(struct http_response_line *res_line)
{
    char tmp_str[MAX_KEY_STR_LEN] = {0};
    memcpy(tmp_str, res_line->version.str, res_line->version.str_len);
    printf("res_version:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    memcpy(tmp_str, res_line->status.str, res_line->status.str_len);
    printf("res_status:%s\n", tmp_str);
}

void output_http_header(struct http_header *header)
{
    char tmp_key[MAX_KEY_STR_LEN] = {0};
    char tmp_val[MAX_KEY_STR_LEN] = {0};

    memcpy(tmp_key, header->key.str, header->key.str_len);
    memcpy(tmp_val, header->val.str, header->val.str_len);
    printf("<%s:%s>\n", tmp_key, tmp_val);
}

void output_http_body(struct hstring *body, int decompress_flag)
{
    int counter = 0;

    if (1 == decompress_flag)
    {
        printf("\n\n----------------decompress body len:%zu---------------\n",
               body->str_len);
    }
    else
    {
        printf("\n\n----------------raw body len:%zu---------------\n",
               body->str_len);
    }

    for (size_t i = 0; i < body->str_len; i++)
    {
        if (counter % 16 == 0)
        {
            printf("\n");
        }
        printf("%02x ", (unsigned char)body->str[i]);
        counter++;
    }
    printf("\n");
}
#endif

static void append_http_payload(struct session *sess, struct gtest_plug_exdata_t *gtest_plug_exdata, const struct hstring *body, enum http_transaction_type type)
{
    if (NULL == body->str || 0 == body->str_len)
    {
        return;
    }
    if (NULL == gtest_plug_exdata->md5ctx[type])
    {
        gtest_plug_exdata->md5ctx[type] = MMALLOC(MD5_CTX, sizeof(MD5_CTX));
        MD5Init(gtest_plug_exdata->md5ctx[type]);
    }
    MD5Update(gtest_plug_exdata->md5ctx[type], (unsigned char *)body->str, body->str_len);
}

int http_field_to_json(cJSON *object, const char *key, char *val, size_t val_len)
{
    if (NULL == object || NULL == key || NULL == val || 0 == val_len)
    {
        return -1;
    }

    char *tmp = CALLOC(char, val_len + 1);
    memcpy(tmp, val, val_len);
    cJSON_AddStringToObject(object, key, tmp);
    FREE(tmp);

    return 0;
}

void transaction_index_to_json(cJSON *ctx, int transaction_index)
{
    cJSON_AddNumberToObject(ctx, GTEST_HTTP_TRANS_SEQ_NAME, transaction_index);
}

void req_line_to_json(cJSON *ctx, struct http_request_line *req_line)
{
    http_field_to_json(ctx, "method", req_line->method.str,
                       req_line->method.str_len);
    http_field_to_json(ctx, "uri", req_line->uri.str, req_line->uri.str_len);
    http_field_to_json(ctx, "req_version", req_line->version.str,
                       req_line->version.str_len);

    cJSON_AddNumberToObject(ctx, "major_version", req_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", req_line->minor_version);
}

void res_line_to_json(cJSON *ctx, struct http_response_line *res_line)
{
    http_field_to_json(ctx, "res_version", res_line->version.str,
                       res_line->version.str_len);
    http_field_to_json(ctx, "res_status", res_line->status.str,
                       res_line->status.str_len);

    cJSON_AddNumberToObject(ctx, "major_version", res_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", res_line->minor_version);
    cJSON_AddNumberToObject(ctx, "status_code", res_line->status_code);
}

void http_header_to_json(cJSON *ctx, struct http_header *header)
{
    char key[MAX_KEY_STR_LEN] = {0};

    memcpy(key, header->key.str, header->key.str_len);

    if (cJSON_HasObjectItem(ctx, key) == FALSE)
    {
        http_field_to_json(ctx, key, header->val.str, header->val.str_len);
    }
    else
    {
        // ctx already has the key, so rename key by key%d
        char new_key[MAX_KEY_STR_LEN] = {0};
        sprintf(new_key, "%s%d", key, g_header_count++);
        http_field_to_json(ctx, new_key, header->val.str, header->val.str_len);
    }
}

void http_url_add_to_json(cJSON *ctx, struct http_message *msg)
{
    struct hstring url_result = {};

    if (cJSON_GetObjectItem(ctx, GTEST_HTTP_URL_NAME))
    {
        return;
    }

    if (http_message_get_url(msg, &url_result) < 0)
    {
        // printf("url:%s\n", url_result.str);
        return;
    }

    struct http_header url_header_result = {};

    url_header_result.key.str = (char *)GTEST_HTTP_URL_NAME;
    url_header_result.key.str_len = strlen(GTEST_HTTP_URL_NAME);
    url_header_result.val = url_result;

    http_header_to_json(ctx, &url_header_result);
}

// Full duplex
static void commit_last_half_flow_data(struct session *sess, struct gtest_plug_exdata_t *gtest_plug_exdata,
                                       struct http_message *msg, enum http_transaction_type type, int final)
{
    char result_name[MAX_KEY_STR_LEN] = {0};

    cJSON *last_jnode = gtest_plug_exdata->result_jnode[type];
    if (last_jnode)
    {
        // finish md5 streming hash
        if (gtest_plug_exdata->md5ctx[type])
        {
            unsigned char md5_result_bin[16] = {0};
            unsigned char md5_result_cstr[33] = {0};
            MD5Final(md5_result_bin, gtest_plug_exdata->md5ctx[type]);

            for (int i = 0; i < 16; i++)
                sprintf((char *)md5_result_cstr + 2 * i, "%02x", md5_result_bin[i]);
            md5_result_cstr[32] = '\0';

            cJSON_AddStringToObject(last_jnode, GTEST_HTTP_PAYLOAD_MD5_NAME, (char *)md5_result_cstr);
            FREE(gtest_plug_exdata->md5ctx[type]);
            gtest_plug_exdata->md5ctx[type] = NULL;
        }

        sprintf(result_name, "%d", g_result_count);
        commit_test_result_json(last_jnode, result_name);
        gtest_plug_exdata->result_jnode[type] = NULL;
        g_result_count++;
    }

    if(0 == final){
        gtest_plug_exdata->result_jnode[type] = cJSON_CreateObject();
        if (HTTP_TRANSACTION_REQ == type)
        {
            cJSON_AddStringToObject(gtest_plug_exdata->result_jnode[type], GTEST_HTTP_TRANS_NAME, "request");
        }
        else if (HTTP_TRANSACTION_RES == type)
        {
            cJSON_AddStringToObject(gtest_plug_exdata->result_jnode[type], GTEST_HTTP_TRANS_NAME, "response");
        }
        if (msg)
        {
            transaction_index_to_json(gtest_plug_exdata->result_jnode[type], http_message_get_transaction_seq(msg));
        }
    }
}

static void http_decoder_test_update_session_tuple4(struct session *sess, struct gtest_plug_exdata_t *gtest_plug_exdata)
{
    if (gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION] == NULL)
    {
        const char *human_addr_cstr = session_get0_readable_addr(sess);
        if(NULL == human_addr_cstr){
            fprintf(stderr, "can't get readable_addr, to use session_get0_readable_addr() the sapp_log.conf level must <= INFO\n");
            return;
        }
        char result_name[MAX_KEY_STR_LEN] = {0};
        gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION] = cJSON_CreateObject();
        cJSON_AddStringToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION], GTEST_HTTP_TUPLE4_NAME, human_addr_cstr);
        sprintf(result_name, "%d", g_result_count++);
        commit_test_result_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION], result_name);
    }
}

extern "C" void http_decoder_test_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env)
// extern "C" int http_decoder_test_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    struct http_request_line req_line = {0};
    struct http_response_line res_line = {0};
    struct http_header header = {0};
    struct hstring body = {0};
    struct http_message *msg = (struct http_message *)raw_msg;
    enum http_message_type msg_type = http_message_type(msg);

    struct gtest_plug_exdata_t *gtest_plug_exdata = (struct gtest_plug_exdata_t *)session_exdata_get(sess, g_exdata_idx);
    if (NULL == gtest_plug_exdata)
    {
        gtest_plug_exdata = (struct gtest_plug_exdata_t *)calloc(1, sizeof(struct gtest_plug_exdata_t));
        session_exdata_set(sess, g_exdata_idx, gtest_plug_exdata);
    }

    if (msg_type == HTTP_MESSAGE_REQ_LINE || msg_type == HTTP_MESSAGE_REQ_HEADER || msg_type == HTTP_MESSAGE_REQ_BODY)
    {
        cJSON *json = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ];
    }
    else
    {
        cJSON *json = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES];
    }

    http_decoder_test_update_session_tuple4(sess, gtest_plug_exdata);

    switch (msg_type)
    {
    case HTTP_MESSAGE_REQ_LINE:
        commit_last_half_flow_data(sess, gtest_plug_exdata, msg, HTTP_TRANSACTION_REQ, 0);
        http_message_get_request_line(msg, &req_line);
        req_line_to_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ], &req_line);
        break;
    case HTTP_MESSAGE_REQ_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            http_header_to_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ], &header);
        }
        g_header_count = 1;
        http_url_add_to_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ], msg);
        break;
    case HTTP_MESSAGE_REQ_BODY:
        // http_message_get_request_raw_body(msg, &body);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        append_http_payload(sess, gtest_plug_exdata, &body, HTTP_TRANSACTION_REQ);
        break;
    case HTTP_MESSAGE_RES_LINE:
        commit_last_half_flow_data(sess, gtest_plug_exdata, msg, HTTP_TRANSACTION_RES, 0);
        http_message_get_response_line(msg, &res_line);
        res_line_to_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES], &res_line);
        break;
    case HTTP_MESSAGE_RES_HEADER:
        while (http_message_header_next(msg, &header) >= 0)
        {
            http_header_to_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES], &header);
        }
        g_header_count = 1;
        break;
    case HTTP_MESSAGE_RES_BODY:
        // http_message_get_response_raw_body(msg, &body);
        //  output_http_body(&body, 0);
        http_message_get_decompress_body(msg, &body);
        // output_http_body(&body, 1);
        append_http_payload(sess, gtest_plug_exdata, &body, HTTP_TRANSACTION_RES);
        break;

        // to do: check payload
    default:
        break;
    }

    return;
}

void http_decoder_test_exdata_free(struct session *sess, int idx, void *ex_ptr, void *arg)
{
    if (ex_ptr != NULL)
    {
        struct gtest_plug_exdata_t *gtest_plug_exdata = (struct gtest_plug_exdata_t *)ex_ptr;
        if (gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ])
        {
            commit_last_half_flow_data(sess, gtest_plug_exdata, NULL, HTTP_TRANSACTION_REQ, 1);
        }
        if (gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES])
        {
            commit_last_half_flow_data(sess, gtest_plug_exdata, NULL, HTTP_TRANSACTION_RES, 1);
        }
        free(ex_ptr);
    }
}

extern "C" void *http_decoder_test_init(struct stellar *st)
{
    g_http_gtest_plugin_id = stellar_session_plugin_register(st, NULL, NULL, NULL);
    g_exdata_idx = stellar_session_exdata_new_index(st, "HTTP_DECODER_REQ_TEST",
                                         http_decoder_test_exdata_free,
                                                NULL);
    if (g_exdata_idx < 0)
    {
        printf("[%s:%d]: can't get http_decoder exdata index !!!\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    g_topic_id = stellar_session_mq_get_topic_id(st, HTTP_DECODER_TOPIC);
    if (g_topic_id < 0)
    {
        printf("[%s:%d]: can't get http_decoder topic id !!!\n", __FUNCTION__, __LINE__);
        return NULL;
    }
    stellar_session_mq_subscribe(st, g_topic_id, g_entry_fun, g_http_gtest_plugin_id);
    printf("http_decoder_test_init succ, plugin_id:%d, topic_id:%d\n", g_http_gtest_plugin_id, g_topic_id);

    return (void *)strdup("http_decoder_test_ctx");
}

extern "C" void http_decoder_test_exit(void *test_ctx)
{
    if (test_ctx != NULL)
    {
        FREE(test_ctx);
    }
    printf("http_decoder_test_exit OK!\n");
}

extern "C" void http_decoder_plug_set_entry_fuc(on_msg_cb_func *entry_fun)
{
    g_entry_fun = entry_fun;
}