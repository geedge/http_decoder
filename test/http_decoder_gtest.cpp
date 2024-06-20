#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "http_decoder_inc.h"

#ifdef __cplusplus
extern "C"
{
#include "cJSON.h"
#include "http_decoder_gtest.h"
#include "md5.h"
#include "toml/toml.h"

    int commit_test_result_json(cJSON *node, const char *name);
    extern void http_decoder_test_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env);
    extern void http_decoder_test_state_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env);
    extern void http_decoder_tunnel_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env);
    static on_session_msg_cb_func *g_entry_fun = http_decoder_test_entry;
}
#endif

#define MAX_KEY_STR_LEN 2048
#define GTEST_PLUG_ENTRY_CFG  "./etc/http/gtest_entry.toml"

struct plug_entry_t{
    const char *name;
    on_session_msg_cb_func *entry;
};

static struct plug_entry_t g_entry_tbl[] = {
    {"http_decoder_test_entry", http_decoder_test_entry},
    {"http_decoder_test_state_entry", http_decoder_test_state_entry},
    {"http_decoder_tunnel_entry", http_decoder_tunnel_entry},
    {NULL, NULL}
};

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
    memcpy(tmp_str, req_line->method.iov_base, req_line->method.iov_len);
    printf("req_method:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    memcpy(tmp_str, req_line->uri.iov_base, req_line->uri.iov_len);
    printf("req_uri:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    memcpy(tmp_str, req_line->version.iov_base, req_line->version.iov_len);
    printf("req_version:%s\n", tmp_str);
}

void output_http_res_line(struct http_response_line *res_line)
{
    char tmp_str[MAX_KEY_STR_LEN] = {0};
    memcpy(tmp_str, res_line->version.iov_base, res_line->version.iov_len);
    printf("res_version:%s\n", tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    memcpy(tmp_str, res_line->status.iov_base, res_line->status.iov_len);
    printf("res_status:%s\n", tmp_str);
}

void output_http_header(struct http_header *header)
{
    char tmp_key[MAX_KEY_STR_LEN] = {0};
    char tmp_val[MAX_KEY_STR_LEN] = {0};

    memcpy(tmp_key, header->key.iov_base, header->key.iov_len);
    memcpy(tmp_val, header->val.iov_base, header->val.iov_len);
    printf("<%s:%s>\n", tmp_key, tmp_val);
}

void output_http_body(hstring *body, int decompress_flag)
{
    int counter = 0;

    if (1 == decompress_flag)
    {
        printf("\n\n----------------decompress body len:%zu---------------\n",
               body->iov_len);
    }
    else
    {
        printf("\n\n----------------raw body len:%zu---------------\n",
               body->iov_len);
    }

    for (size_t i = 0; i < body->iov_len; i++)
    {
        if (counter % 16 == 0)
        {
            printf("\n");
        }
        printf("%02x ", (unsigned char)body->iov_base[i]);
        counter++;
    }
    printf("\n");
}
#endif

static void append_http_payload(struct session *sess, struct gtest_plug_exdata_t *gtest_plug_exdata, const hstring *body, enum http_transaction_type type)
{
    if (NULL == body->iov_base || 0 == body->iov_len)
    {
        return;
    }
    if (NULL == gtest_plug_exdata->md5ctx[type])
    {
        gtest_plug_exdata->md5ctx[type] = MMALLOC(MD5_CTX, sizeof(MD5_CTX));
        MD5Init(gtest_plug_exdata->md5ctx[type]);
    }
    MD5Update(gtest_plug_exdata->md5ctx[type], (unsigned char *)body->iov_base, body->iov_len);
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
    http_field_to_json(ctx, "method", (char *)req_line->method.iov_base,
                       req_line->method.iov_len);
    http_field_to_json(ctx, "uri", (char *)req_line->uri.iov_base, req_line->uri.iov_len);
    http_field_to_json(ctx, "req_version", (char *)req_line->version.iov_base,
                       req_line->version.iov_len);

    cJSON_AddNumberToObject(ctx, "major_version", req_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", req_line->minor_version);
}

void res_line_to_json(cJSON *ctx, struct http_response_line *res_line)
{
    http_field_to_json(ctx, "res_version", (char *)res_line->version.iov_base,
                       res_line->version.iov_len);
    http_field_to_json(ctx, "res_status", (char *)res_line->status.iov_base,
                       res_line->status.iov_len);

    cJSON_AddNumberToObject(ctx, "major_version", res_line->major_version);
    cJSON_AddNumberToObject(ctx, "minor_version", res_line->minor_version);
    cJSON_AddNumberToObject(ctx, "status_code", res_line->status_code);
}

void http_header_to_json(cJSON *ctx, struct http_header *header)
{
    char key[MAX_KEY_STR_LEN] = {0};
    assert(header->key.iov_base);
    assert(header->val.iov_base);

    memcpy(key, header->key.iov_base, header->key.iov_len);

    if (cJSON_HasObjectItem(ctx, key) == FALSE)
    {
        http_field_to_json(ctx, key, (char *)header->val.iov_base, header->val.iov_len);
    }
    else
    {
        // ctx already has the key, so rename key by key%d
        char new_key[MAX_KEY_STR_LEN] = {0};
        sprintf(new_key, "%s%d", key, g_header_count++);
        http_field_to_json(ctx, new_key, (char *)header->val.iov_base, header->val.iov_len);
    }
}

void http_url_add_to_json(cJSON *ctx, struct http_message *msg)
{
    hstring url_result = {};

    if (cJSON_GetObjectItem(ctx, GTEST_HTTP_URL_NAME))
    {
        return;
    }

    http_message_get_url(msg, &url_result);
    if(url_result.iov_base == NULL)
    {
        // printf("url:%s\n", url_result.iov_base);
        return;
    }

    struct http_header url_header_result = {};

    url_header_result.key.iov_base = (char *)GTEST_HTTP_URL_NAME;
    url_header_result.key.iov_len = strlen(GTEST_HTTP_URL_NAME);
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

static int get_gtest_plug_entry(const char *cfg_path, char *entry_name, int max_len, char *topic_name, int topic_max_len)
{
    FILE *fp = fopen(cfg_path, "r");
    if (NULL == fp)
    {
        fprintf(stderr, "[%s:%d]Can't open config file:%s",
                __FUNCTION__, __LINE__, cfg_path);
        return -1;
    }

    int ret = 0;
    char errbuf[256] = {0};

    toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    toml_table_t *basic_sec_tbl = toml_table_in(root, "entry");
    if (NULL == basic_sec_tbl)
    {
        fprintf(stderr, "[%s:%d]config file:%s has no key: [entry]",
                __FUNCTION__, __LINE__, cfg_path);
        toml_free(root);
        return -1;
    }

    toml_datum_t str_val = toml_string_in(basic_sec_tbl, "name");
    if (str_val.ok != 0)
    {
        strncpy(entry_name, str_val.u.s, max_len);
        free(str_val.u.s);
    }

    toml_datum_t topic_str_val = toml_string_in(basic_sec_tbl, "topic");
    if (str_val.ok != 0)
    {
        strncpy(topic_name, topic_str_val.u.s, topic_max_len);
        free(topic_str_val.u.s);
    }

    toml_free(root);
    return 0;
}

static void set_gtest_plug_entry(const char *entry_name)
{
    if(NULL == entry_name){
        g_entry_fun = http_decoder_test_entry; //set default
        return;
    }
    for (size_t i = 0; g_entry_tbl[i].name != NULL; i++)
    {
        if(strcmp(entry_name, g_entry_tbl[i].name) == 0){
            g_entry_fun = g_entry_tbl[i].entry;
            return;
        }
    }
    g_entry_fun = http_decoder_test_entry; //set default
}

extern "C" void http_decoder_test_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env)
{
    struct http_request_line req_line = {0};
    struct http_response_line res_line = {0};
    struct http_header header = {0};
    hstring body = {0};
    struct http_message *msg = (struct http_message *)raw_msg;
    enum http_message_type msg_type = http_message_type_get(msg);

    struct gtest_plug_exdata_t *gtest_plug_exdata = (struct gtest_plug_exdata_t *)session_exdata_get(sess, g_exdata_idx);
    if (NULL == gtest_plug_exdata)
    {
        gtest_plug_exdata = (struct gtest_plug_exdata_t *)calloc(1, sizeof(struct gtest_plug_exdata_t));
        session_exdata_set(sess, g_exdata_idx, gtest_plug_exdata);
    }

    if (http_message_type_is_req(sess, msg_type))
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

static int update_config_file(const char *filename, const char *key, const char *value)
{
	char cmd_buf[1024] = {};
	snprintf(cmd_buf, 1024, "sed 's/^[ \t]*%s=.*/%s=%s/g' -i %s", key, key, value, filename);
	int ret = system(cmd_buf);
	return ret;
}

extern "C" void http_decoder_test_state_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env)
{
    static int msg_index = 0;
    char msg_index_name[64] = {};
    char msg_index_value[64] = {};
    cJSON *json_object = NULL;
    enum http_message_type msg_type = http_message_type_get((struct http_message *)raw_msg);

    struct gtest_plug_exdata_t *gtest_plug_exdata = (struct gtest_plug_exdata_t *)session_exdata_get(sess, g_exdata_idx);
    if (NULL == gtest_plug_exdata)
    {
        gtest_plug_exdata = (struct gtest_plug_exdata_t *)calloc(1, sizeof(struct gtest_plug_exdata_t));
        gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ] = cJSON_CreateObject();
        gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES] = cJSON_CreateObject();
        session_exdata_set(sess, g_exdata_idx, gtest_plug_exdata);
    }

    if (http_message_type_is_req(sess, msg_type)){
        json_object = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ];
    }else{
        json_object = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES];
    }
    if(HTTP_TRANSACTION_FREE == msg_type){
        unsigned char flow_flag;
        session_is_symmetric(sess, &flow_flag);
        if(SESSION_SEEN_C2S_FLOW == flow_flag){
            json_object = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ];
        }
        else if(SESSION_SEEN_S2C_FLOW == flow_flag){
            json_object = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES];
        }else{ // is symmetric
            json_object = gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES];
        }
    }
    int cur_transaction_id = http_message_get_transaction_seq((const http_message* )raw_msg);

    if(HTTP_TRANSACTION_NEW ==msg_type || HTTP_TRANSACTION_FREE == msg_type){
        snprintf(msg_index_name, sizeof(msg_index_name), "msg_%d", msg_index++);
        snprintf(msg_index_value, sizeof(msg_index_value), "%s_transaction_%d", http_message_type_to_string(msg_type), cur_transaction_id);   
        cJSON_AddStringToObject(json_object, msg_index_name, msg_index_value);   
    }else{
        snprintf(msg_index_name, sizeof(msg_index_name), "msg_%d", msg_index++);
        cJSON_AddStringToObject(json_object, msg_index_name, http_message_type_to_string(msg_type));
    }
    return;
}

extern "C" void http_decoder_tunnel_entry(struct session *sess, int topic_id, const void *raw_msg, void *no_use_ctx, void *plugin_env)
{
    struct gtest_plug_exdata_t *gtest_plug_exdata;
    enum http_tunnel_message_type tmsg_type = http_tunnel_message_type_get((const struct http_tunnel_message *)raw_msg);
    static size_t req_payload_block = 0, req_payload_size = 0;
    static size_t res_payload_block = 0, res_payload_size = 0;
    gtest_plug_exdata = (struct gtest_plug_exdata_t *)session_exdata_get(sess, g_exdata_idx);

    switch(tmsg_type){
        case HTTP_TUNNEL_OPENING:
        {
            if (NULL == gtest_plug_exdata)
            {
                gtest_plug_exdata = (struct gtest_plug_exdata_t *)calloc(1, sizeof(struct gtest_plug_exdata_t));
                session_exdata_set(sess, g_exdata_idx, gtest_plug_exdata);
            }
            const char *human_addr_cstr = session_get0_readable_addr(sess);
            gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ] = cJSON_CreateObject();
            gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES] = cJSON_CreateObject();
            gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION] = cJSON_CreateObject();
            cJSON_AddStringToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION], GTEST_HTTP_TUPLE4_NAME, human_addr_cstr);
            commit_test_result_json(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_SESSION], "TUNNEL_NEW");
        }
        // OPENING state has payload, go on

        case HTTP_TUNNEL_ACTIVE:
        {
            int curdir = packet_get_direction(session_get0_current_packet(sess));
            hstring tunnel_payload = {};
            http_tunnel_message_get_payload((const struct http_tunnel_message *)raw_msg, &tunnel_payload);
            if(PACKET_DIRECTION_C2S == curdir){
                req_payload_block++;
                req_payload_size += tunnel_payload.iov_len;
            }else{
                res_payload_block++;
                res_payload_size += tunnel_payload.iov_len;
            }
        }
        break;
        case HTTP_TUNNEL_CLOSING:
        {
            cJSON_AddStringToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ], "flow", "C2S");
             cJSON_AddStringToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES], "flow", "S2C");
            cJSON_AddNumberToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ], "payload_block", req_payload_block);
            cJSON_AddNumberToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_REQ], "payload_size", req_payload_size);
            cJSON_AddNumberToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES], "payload_block", res_payload_block);
            cJSON_AddNumberToObject(gtest_plug_exdata->result_jnode[HTTP_TRANSACTION_RES], "payload_size", res_payload_size);
        }
        break;
        default:
            assert(0);
            break;
    }
}

extern "C" void *http_decoder_test_init(struct stellar *st)
{
    g_http_gtest_plugin_id = stellar_session_plugin_register(st, NULL, NULL, NULL);
    g_exdata_idx = stellar_session_exdata_new_index(st, "HTTP_DECODER_GTEST_EXDATA",http_decoder_test_exdata_free, NULL);
    if (g_exdata_idx < 0)
    {
        printf("[%s:%d]: can't get http_decoder exdata index !!!\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    char entry_name[64] = "";
    char topic_name[64] = "";
    get_gtest_plug_entry(GTEST_PLUG_ENTRY_CFG, entry_name, sizeof(entry_name)-1, topic_name, sizeof(topic_name)-1);
    set_gtest_plug_entry(entry_name);
    g_topic_id = stellar_session_mq_get_topic_id(st, topic_name);
    if (g_topic_id < 0)
    {
        printf("[%s:%d]: can't get http_decoder topic:%s id !!!\n", __FUNCTION__, __LINE__, topic_name);
        return NULL;
    }    
    stellar_session_mq_subscribe(st, g_topic_id, g_entry_fun, g_http_gtest_plugin_id);
    printf("http_decoder_gtest_init succ, plugin_id:%d, sub_topic_id:%d\n", g_http_gtest_plugin_id, g_topic_id);

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