#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "http_decoder_inc.h"

struct http_message *http_message_new(enum http_message_type type, struct http_decoder_result_queue *queue,
                                        int queue_index, uint8_t flow_type)
{
    struct http_message *msg = CALLOC(struct http_message, 1);
    msg->type = type;
    msg->ref_queue = queue;
    msg->queue_index = queue_index;
    msg->flow_type = flow_type;
    return msg;
}

static void http_message_free(struct session *sess, void *http_msg, void *cb_arg)
{
    if (http_msg)
    {
        FREE(http_msg);
    }
}

static void http_event_handler(enum http_event event, struct http_decoder_half_data **data,
                               struct http_event_context *ev_ctx, void *httpd_plugin_env)
{
    assert(ev_ctx);
    assert(httpd_plugin_env);
    struct http_decoder_env *httpd_env = (struct http_decoder_env *)httpd_plugin_env;
    size_t queue_idx = 0;
    nmx_pool_t *mempool = ev_ctx->ref_mempool;
    struct http_decoder_result_queue *queue = ev_ctx->ref_queue;
    struct http_message *msg = NULL;
    struct http_decoder_half_data *half_data = NULL;
    int ret = 0;
    u_int8_t flow_flag = 0;

    int thread_id = stellar_get_current_thread_id(httpd_env->st);

    if(http_event_is_req(event)){
        queue_idx = http_decoder_result_queue_req_index(queue);
        half_data = http_decoder_result_queue_peek_req(queue);
    }else{
        queue_idx = http_decoder_result_queue_res_index(queue);
        half_data = http_decoder_result_queue_peek_res(queue);
    }

    switch (event)
    {
    case HTTP_EVENT_REQ_INIT:
        half_data = http_decoder_result_queue_peek_req(queue);
        if (half_data != NULL)
        {
            http_decoder_result_queue_inc_req_index(queue);
        }

        half_data = http_decoder_result_queue_peek_req(queue);
        if (half_data != NULL)
        {
            half_data = http_decoder_result_queue_pop_req(queue);
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
        }

        half_data = http_decoder_half_data_new(mempool);
        ret = http_decoder_result_queue_push_req(queue, half_data);
        if (ret < 0)
        {
            fprintf(stderr, "http_decoder_result_queue_push req failed.");
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
        }
        *data = half_data;
        queue_idx = http_decoder_result_queue_req_index(queue); //get the index after inc
#if 1 /* llhttp always call on_message_begin() even if llhttp_execute() error!!! */
        msg = http_message_new(HTTP_TRANSACTION_NEW, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);      
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TRANSACTION_NEW, 1);  
#endif
        break;
    case HTTP_EVENT_REQ_LINE:
#if 0
        msg = http_message_new(HTTP_TRANSACTION_NEW, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);      
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TRANSACTION_NEW, 1);  
#endif
        msg = http_message_new(HTTP_MESSAGE_REQ_LINE, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_HDR:
        msg = http_message_new(HTTP_MESSAGE_REQ_HEADER, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
    break;
    case HTTP_EVENT_REQ_HDR_END:
    {
        http_decoder_join_url_finally(ev_ctx, http_decoder_result_queue_peek_req(queue), mempool);
        /*  maybe some parsed headers in buffer, but has not pushed to plugins yet */
        half_data = http_decoder_result_queue_peek_req(queue);
        if(http_decoder_half_data_has_parsed_header(half_data)){
            msg = http_message_new(HTTP_MESSAGE_REQ_HEADER, queue, queue_idx, HTTP_REQUEST);
            session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        }
        http_half_data_update_commit_index(half_data);
        msg = http_message_new(HTTP_MESSAGE_REQ_HEADER_END, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);       

        int tot_c2s_headers = http_half_data_get_total_parsed_header_count(half_data); 
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_HEADERS_C2S, tot_c2s_headers);

        struct hstring tmp_url = {};
        http_half_data_get_url(half_data, &tmp_url);
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_URL_BYTES, tmp_url.str_len);
    }
    break;
    case HTTP_EVENT_REQ_BODY_BEGIN:
        //do nothing, in BODY_DATA event
        break;
    case HTTP_EVENT_REQ_BODY_DATA:
        msg = http_message_new(HTTP_MESSAGE_REQ_BODY, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_END:
        msg = http_message_new(HTTP_MESSAGE_REQ_BODY_END, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_END:
    {
        session_is_symmetric(ev_ctx->ref_session, &flow_flag);
        if(SESSION_SEEN_C2S_FLOW == flow_flag){
            msg = http_message_new(HTTP_TRANSACTION_FREE, queue, queue_idx, HTTP_REQUEST);
            session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TRANSACTION_FREE, 1);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_ASYMMETRY_TRANSACTION_C2S, 1);
        }
        http_half_update_state(half_data, event);
        http_decoder_result_queue_inc_req_index(queue);
        half_data = http_decoder_result_queue_pop_req(queue);
        if (half_data != NULL)
        {
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
        }
    }
        break;

    case HTTP_EVENT_RES_INIT:
        half_data = http_decoder_result_queue_peek_res(queue);
        if (half_data != NULL)
        {
            http_decoder_result_queue_inc_res_index(queue);
        }

        half_data = http_decoder_result_queue_peek_res(queue);
        if (half_data != NULL)
        {
            half_data = http_decoder_result_queue_pop_res(queue);
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
        }

        half_data = http_decoder_half_data_new(mempool);
        ret = http_decoder_result_queue_push_res(queue, half_data);
        if (ret < 0)
        {
            fprintf(stderr, "http_decoder_result_queue_push res failed.");
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
        }
        queue_idx = http_decoder_result_queue_res_index(queue); //get the index after inc
        *data = half_data;
        if(0 == session_is_symmetric(ev_ctx->ref_session, &flow_flag)){
            if(SESSION_SEEN_S2C_FLOW == flow_flag){
                msg = http_message_new(HTTP_TRANSACTION_NEW, queue, queue_idx, HTTP_RESPONSE);
                session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);    
            }
        }
        break;
    case HTTP_EVENT_RES_LINE:
        msg = http_message_new(HTTP_MESSAGE_RES_LINE, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_HDR:
        msg = http_message_new(HTTP_MESSAGE_RES_HEADER, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_HDR_END:
    {
        /*  maybe some header in table buffer but has not pushed to plugins */
        half_data = http_decoder_result_queue_peek_res(queue);
        if(http_decoder_half_data_has_parsed_header(half_data)){
            msg = http_message_new(HTTP_MESSAGE_RES_HEADER, queue, queue_idx, HTTP_RESPONSE);
            session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        }
        http_half_data_update_commit_index(half_data);
        msg = http_message_new(HTTP_MESSAGE_RES_HEADER_END, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);        

        int tot_s2c_headers = http_half_data_get_total_parsed_header_count(half_data); 
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_HEADERS_S2C, tot_s2c_headers);
    }
        break;
    case HTTP_EVENT_RES_BODY_BEGIN:
        break;
    case HTTP_EVENT_RES_BODY_DATA:
        msg = http_message_new(HTTP_MESSAGE_RES_BODY, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_BODY_END:
        msg = http_message_new(HTTP_MESSAGE_RES_BODY_END, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);    
        break;
    case HTTP_EVENT_RES_END:
        msg = http_message_new(HTTP_TRANSACTION_FREE, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TRANSACTION_FREE, 1); 
        session_is_symmetric(ev_ctx->ref_session, &flow_flag);
        if(SESSION_SEEN_S2C_FLOW == flow_flag){
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_ASYMMETRY_TRANSACTION_S2C, 1);
        }
        http_half_update_state(half_data, event);
        http_decoder_result_queue_inc_res_index(queue);
        half_data = http_decoder_result_queue_pop_res(queue);
        if (half_data != NULL)
        {
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
        }
        break;
    default:
        assert(0);
        break;
    }
    if(half_data){
        http_half_update_state(half_data, event);
    }
}

static struct http_decoder *http_decoder_new(nmx_pool_t *mempool, http_event_cb *ev_cb, int decompress_switch, struct http_decoder_env *httpd_env)
{
    struct http_decoder *decoder = MEMPOOL_CALLOC(mempool, struct http_decoder, 1);
    assert(decoder);
    decoder->c2s_half = http_decoder_half_new(mempool, ev_cb, HTTP_REQUEST,  decompress_switch, httpd_env);
    decoder->s2c_half = http_decoder_half_new(mempool, ev_cb, HTTP_RESPONSE, decompress_switch, httpd_env);
    return decoder;
}

static void http_decoder_free(nmx_pool_t *mempool, struct http_decoder *decoder)
{
    if (NULL == decoder)
    {
        return;
    }
    if (decoder->c2s_half != NULL)
    {
        http_decoder_half_free(mempool, decoder->c2s_half);
        decoder->c2s_half = NULL;
    }
    if (decoder->s2c_half != NULL)
    {
        http_decoder_half_free(mempool, decoder->s2c_half);
        decoder->s2c_half = NULL;
    }
    MEMPOOL_FREE(mempool, decoder);
}

static struct http_decoder_exdata *http_decoder_exdata_new(size_t mempool_size, size_t queue_size,
                                        int decompress_switch, struct http_decoder_env *httpd_env)
{
    struct http_decoder_exdata *ex_data = CALLOC(struct http_decoder_exdata, 1);
    ex_data->mempool = nmx_create_pool(mempool_size);
    ex_data->decoder = http_decoder_new(ex_data->mempool, http_event_handler, decompress_switch, httpd_env);
    ex_data->queue = http_decoder_result_queue_new(ex_data->mempool, queue_size);
    return ex_data;
}

static void http_decoder_exdata_free(struct http_decoder_exdata *ex_data)
{
    if (unlikely(NULL == ex_data))
    {
        return;
    }
    if (ex_data->decoder != NULL)
    {
        http_decoder_free(ex_data->mempool, ex_data->decoder);
        ex_data->decoder = NULL;
    }
    if (ex_data->queue != NULL)
    {
        http_decoder_result_queue_free(ex_data->mempool, ex_data->queue);
        ex_data->queue = NULL;
    }

    nmx_destroy_pool(ex_data->mempool);
    FREE(ex_data);
}

static int http_protocol_identify(const char *data, size_t data_len)
{
    llhttp_t parser;
    llhttp_settings_t settings;
    enum llhttp_errno error;

    llhttp_settings_init(&settings);
    llhttp_init(&parser, HTTP_BOTH, &settings);

    error = llhttp_execute(&parser, data, data_len);
    if (error != HPE_OK)
    {
        return -1;
    }
    return 0;
}

static void _http_decoder_context_free(struct http_decoder_env *env)
{
    if (NULL == env)
    {
        return;
    }
    if (env->hd_stat.fse != NULL)
    {
        fieldstat_easy_free(env->hd_stat.fse);
        env->hd_stat.fse = NULL;
    }

    http_decoder_stat_free(&env->hd_stat);

    if (env->httpd_msg_topic_id >= 0)
    {
        stellar_session_mq_destroy_topic(env->st, env->httpd_msg_topic_id);
        env->httpd_msg_topic_id = -1;
    }

    FREE(env);
}

static int load_http_decoder_config(const char *cfg_path,
                                    struct http_decoder_config *hd_cfg)
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

    toml_table_t *basic_sec_tbl = toml_table_in(root, "basic");
    if (NULL == basic_sec_tbl)
    {
        fprintf(stderr, "[%s:%d]config file:%s has no key: [basic]",
                __FUNCTION__, __LINE__, cfg_path);
        toml_free(root);
        return -1;
    }

    toml_datum_t int_val = toml_int_in(basic_sec_tbl, "decompress");
    if (int_val.ok != 0)
    {
        hd_cfg->decompress_switch = int_val.u.b;
    }

    int_val = toml_int_in(basic_sec_tbl, "mempool_size");
    if (int_val.ok != 0)
    {
        hd_cfg->mempool_size = int_val.u.i;
    }
    else
    {
        hd_cfg->mempool_size = DEFAULT_MEMPOOL_SIZE;
    }

    int_val = toml_int_in(basic_sec_tbl, "result_queue_len");
    if (int_val.ok != 0)
    {
        hd_cfg->result_queue_len = int_val.u.i;
    }
    else
    {
        hd_cfg->result_queue_len = HD_RESULT_QUEUE_LEN;
    }

    int_val = toml_int_in(basic_sec_tbl, "stat_interval_pkts");
    if (int_val.ok != 0)
    {
        hd_cfg->stat_interval_pkts = int_val.u.i;
    }
    else
    {
        hd_cfg->stat_interval_pkts = DEFAULT_STAT_INTERVAL_PKTS;
    }

    int_val = toml_int_in(basic_sec_tbl, "stat_output_interval");
    if (int_val.ok != 0)
    {
        hd_cfg->stat_output_interval = int_val.u.i;
    }
    else
    {
        hd_cfg->stat_output_interval = DEFAULT_STAT_OUTPUT_INTERVAL;
    }

    toml_free(root);
    return ret;
}

static int http_msg_get_request_header(const struct http_message *msg, const struct hstring *key,
                                        struct http_header *hdr_result)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_header(req_data, key, hdr_result);
}

static int http_msg_get_response_header(const struct http_message *msg, const struct hstring *key,
                                        struct http_header *hdr_result)
{
    const struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_header(res_data, key, hdr_result);
}

static int http_msg_request_header_next(const struct http_message *msg,
                                        struct http_header *hdr)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_iter_header((struct http_decoder_half_data *)req_data, hdr);
}

static int http_msg_response_header_next(const struct http_message *msg,
                                            struct http_header *hdr)
{
    const struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_iter_header((struct http_decoder_half_data *)res_data, hdr);
}

static int http_msg_get_request_raw_body(const struct http_message *msg,
                                            struct hstring *body)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_raw_body(req_data, body);
}

static int http_msg_get_response_raw_body(const struct http_message *msg,
                                            struct hstring *body)
{
    const struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_raw_body(res_data, body);
}

static int http_msg_get_request_decompress_body(const struct http_message *msg,
                                            struct hstring *body)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_decompress_body(req_data, body);
}

static int http_msg_get_response_decompress_body(const struct http_message *msg,
                                            struct hstring *body)
{
    const struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_decompress_body(res_data, body);
}

#ifdef __cplusplus
extern "C"
{
#endif

    void _httpd_ex_data_free_cb(struct session *s, int idx, void *ex_data, void *arg)
    {
        if (NULL == ex_data)
        {
            return;
        }
        struct http_decoder_exdata *exdata = (struct http_decoder_exdata *)ex_data;
        http_decoder_exdata_free(exdata);
    }

    void *_httpd_session_ctx_new_cb(struct session *sess, void *plugin_env)
    {
        // If not http, ignore this session
        size_t payload_len;
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        const char *payload = session_get0_current_payload(sess, &payload_len);
        if (payload != NULL && payload_len > 0)
        {
            size_t http_identify_len = payload_len > HTTP_IDENTIFY_LEN ? HTTP_IDENTIFY_LEN : payload_len;
            int ret = http_protocol_identify(payload, http_identify_len);
            if (ret < 0)
            {
                stellar_session_plugin_dettach_current_session(sess);
                return (void *)"not_http_session";
            }
        }

        struct http_decoder_exdata *ex_data = http_decoder_exdata_new(httpd_env->hd_cfg.mempool_size,
                                                                      httpd_env->hd_cfg.result_queue_len,
                                                                      httpd_env->hd_cfg.decompress_switch,
                                                                      httpd_env);
        session_exdata_set(sess, httpd_env->ex_data_idx, ex_data);
        int thread_id = stellar_get_current_thread_id(httpd_env->st);
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_SESSION_NEW, 1);  

        return (void *)"fake_http_decoder_ctx"; // http decoder not use ctx, use exdata only!
    }

    void _httpd_session_ctx_free_cb(struct session *sess, void *session_ctx, void *plugin_env)
    {
        if(NULL == plugin_env || NULL == session_ctx){
            return;
        }
        if(strncmp((const char *)session_ctx, "not_http_session", strlen("not_http_session")) == 0){
            return;
        }
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        int thread_id = stellar_get_current_thread_id(httpd_env->st);
        unsigned char flow_flag = 0;
        session_is_symmetric(sess, &flow_flag);

        if(SESSION_SEEN_C2S_FLOW == flow_flag){
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_ASYMMETRY_SESSION_C2S, 1);
        }else if(SESSION_SEEN_S2C_FLOW == flow_flag){
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_ASYMMETRY_SESSION_S2C, 1);
        }else{
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_SESSION_FREE, 1);  
        }
    }

    void http_decoder_tcp_stream_msg_cb(struct session *sess, int topic_id, const void *msg, void *no_use_ctx, void *plugin_env)
    {
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        struct http_decoder_exdata *ex_data = (struct http_decoder_exdata *)session_exdata_get(sess, httpd_env->ex_data_idx);
        size_t payload_len;

        if(SESSION_STATE_CLOSING == session_get_current_state(sess)){
            http_half_pre_context_free(sess, httpd_env, ex_data);
            return;
        }

        const char *payload = session_get0_current_payload(sess, &payload_len);
        if (unlikely(0 == payload_len || NULL == ex_data))
        {
            return;
        }

        int thread_id = stellar_get_current_thread_id(httpd_env->st);
        struct http_decoder_half *cur_half = NULL;

        if (PACKET_DIRECTION_C2S == packet_get_direction(session_get0_current_packet(sess)))
        {
            cur_half = ex_data->decoder->c2s_half;
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_BYTES_C2S, payload_len);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TCP_SEG_C2S, 1);
        }
        else
        {
            cur_half = ex_data->decoder->s2c_half;
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_BYTES_S2C, payload_len);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TCP_SEG_S2C, 1);            
        }

        http_decoder_half_reinit(cur_half, httpd_env->httpd_msg_topic_id, ex_data->queue,
                                 ex_data->mempool, sess);
        int ret = http_decoder_half_parse(cur_half, payload, payload_len);
        if (ret < 0)
        {
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_PARSE_ERR, 1);
            stellar_session_plugin_dettach_current_session(sess);
        }
        return;
    }

    void *http_decoder_init(struct stellar *st)
    {
        int httpd_msg_topic_id = -1, tcp_stream_topic_id = -1;
        int thread_num = 0;

        struct http_decoder_env *httpd_env = CALLOC(struct http_decoder_env, 1);
        int ret = load_http_decoder_config(HTTPD_CFG_FILE, &httpd_env->hd_cfg);
        if (ret < 0)
        {
            goto failed;
        }
        httpd_env->st = st;
        httpd_env->ex_data_idx = stellar_session_exdata_new_index(st, "HTTP_DECODER",
                                                                  _httpd_ex_data_free_cb,
                                                                  NULL);
        httpd_env->plugin_id = stellar_session_plugin_register(st, _httpd_session_ctx_new_cb,
                                                               _httpd_session_ctx_free_cb, (void *)httpd_env);
        if (httpd_env->plugin_id < 0)
        {
            goto failed;
        }

        httpd_msg_topic_id = stellar_session_mq_get_topic_id(st, HTTP_DECODER_TOPIC);
        if (httpd_msg_topic_id < 0)
        {
            httpd_msg_topic_id = stellar_session_mq_create_topic(st, HTTP_DECODER_TOPIC,
                                                                 http_message_free, NULL);
        }
        httpd_env->httpd_msg_topic_id = httpd_msg_topic_id;

        tcp_stream_topic_id = stellar_session_mq_get_topic_id(st, TOPIC_TCP_STREAM);
        assert(tcp_stream_topic_id >= 0);
        stellar_session_mq_subscribe(st, tcp_stream_topic_id, http_decoder_tcp_stream_msg_cb, httpd_env->plugin_id);

        thread_num = stellar_get_worker_thread_num(st);
        assert(thread_num >= 1);
        if (http_decoder_stat_init( &httpd_env->hd_stat, thread_num, 
    httpd_env->hd_cfg.stat_interval_pkts, httpd_env->hd_cfg.stat_output_interval) < 0)
        {
            goto failed;
        }

        printf("http_decoder_init succ: ex_data_idx:%d, plugin_id:%d, topic_id:%d\n",
               httpd_env->ex_data_idx, httpd_env->plugin_id, httpd_env->httpd_msg_topic_id);
        return httpd_env;

    failed:
        fprintf(stderr, "http_decoder_init fail!\n");
        _http_decoder_context_free(httpd_env);
        return NULL;
    }

    void http_decoder_exit(void *plugin_env)
    {
        if (NULL == plugin_env)
        {
            return;
        }
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        _http_decoder_context_free(httpd_env);
    }

    enum http_message_type http_message_type_get(const struct http_message *msg)
    {
        if (unlikely(NULL == msg))
        {
            return HTTP_MESSAGE_MAX;
        }
        return msg->type;
    }

    void http_message_get_request_line(const struct http_message *msg,
                                      struct http_request_line *line)
    {
        if (unlikely(NULL == msg || msg->type != HTTP_MESSAGE_REQ_LINE))
        {
            if(line){ 
                line->method.str = NULL;
                line->uri.str = NULL;
                line->version.str = NULL;
            }
            return;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

        struct http_decoder_half_data *req_data =
            msg->ref_queue->array[msg->queue_index].req_data;

        http_decoder_half_data_get_request_line(req_data, line);
    }

    void http_message_get_response_line(const struct http_message *msg,
                                       struct http_response_line *line)
    {
        if (unlikely(NULL == msg || msg->type != HTTP_MESSAGE_RES_LINE))
        {
            if(line){
                line->version.str = NULL;
                line->status.str = NULL;
            }
            return;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

        struct http_decoder_half_data *res_data =
            msg->ref_queue->array[msg->queue_index].res_data;

        http_decoder_half_data_get_response_line(res_data, line);
    }

    void http_message_get_header(const struct http_message *msg, const struct hstring *key,
                                struct http_header *hdr_result)
    {
        int ret = -1;
        if (unlikely(NULL == msg || NULL == key))
        {
            goto fail;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
        if (HTTP_MESSAGE_REQ_HEADER == msg->type)
        {
            ret = http_msg_get_request_header(msg, key, hdr_result);
        }
        else if (HTTP_MESSAGE_RES_HEADER == msg->type)
        {
            ret = http_msg_get_response_header(msg, key, hdr_result);
        }
        if(ret >= 0){
            return;
        }
    fail:
        if(hdr_result){ 
            hdr_result->key.str = NULL;
            hdr_result->val.str = NULL;
        }
        return;
    }

    int http_message_header_next(const struct http_message *msg,
                                 struct http_header *header)
    {
        int ret =1;
        if (unlikely(NULL == msg))
        {
            goto fail;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
        if (HTTP_MESSAGE_REQ_HEADER == msg->type)
        {
            ret = http_msg_request_header_next(msg, header);
        }
        else if (HTTP_MESSAGE_RES_HEADER == msg->type)
        {
            ret = http_msg_response_header_next(msg, header);
        }
        if(ret < 0){
            goto fail;
        }
        return 0;
    fail:
        if(header){ 
            header->key.str = NULL;
            header->val.str = NULL;
        }
        return -1;
    }

    int http_message_reset_header_iter(struct http_message *msg)
    {
        if (unlikely(NULL == msg))
        {
            return -1;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
        if (HTTP_MESSAGE_REQ_HEADER == msg->type)
        {
            struct http_decoder_half_data *req_data =
                msg->ref_queue->array[msg->queue_index].req_data;
            return http_decoder_half_data_reset_header_iter(req_data);
        }
        else if (HTTP_MESSAGE_RES_HEADER == msg->type)
        {
            struct http_decoder_half_data *res_data =
                msg->ref_queue->array[msg->queue_index].res_data;
            return http_decoder_half_data_reset_header_iter(res_data);
        }
        return -1;
    }

    void http_message_get_raw_body(const struct http_message *msg,
                                  struct hstring *body)
    {
        int ret = -1;
        if (unlikely(NULL == msg))
        {
            goto fail;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
        if (HTTP_MESSAGE_REQ_BODY == msg->type)
        {
            ret = http_msg_get_request_raw_body(msg, body);
        }
        else if (HTTP_MESSAGE_RES_BODY == msg->type)
        {
            ret = http_msg_get_response_raw_body(msg, body);
        }
        if(ret < 0){
            goto fail;
        }
        return;
    fail:
        if(body){ 
            body->str = NULL;
            body->str_len = 0;
        }
        return;
    }

    void http_message_get_decompress_body(const struct http_message *msg,
                                         struct hstring *body)
    {
        int ret = -1;
        if (unlikely(NULL == msg))
        {
            goto fail;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
        if (HTTP_MESSAGE_REQ_BODY == msg->type)
        {
            ret = http_msg_get_request_decompress_body(msg, body);
        }
        else if (HTTP_MESSAGE_RES_BODY == msg->type)
        {
            ret = http_msg_get_response_decompress_body(msg, body);
        }
        if(ret < 0){
            goto fail;
        }
        return;
    fail:
        if(body){ 
            body->str = NULL;
            body->str_len = 0;
        }
        return;    
    }

    void http_message_get_url(const struct http_message *msg, struct hstring *url)
    {
        if (unlikely(NULL == msg))
        {
            if(url){
                url->str = NULL;
                url->str_len = 0;
            }
            return;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

        struct http_decoder_half_data *req_data =
            msg->ref_queue->array[msg->queue_index].req_data;

        if(http_half_data_get_url(req_data, url) < 0){
            goto fail;
        }
        return;

    fail:
        if(url){
            url->str = NULL;
            url->str_len = 0;
        }
        return;    
    }

    int http_message_get_transaction_seq(const struct http_message *msg)
    {
        if (unlikely(NULL == msg))
        {
            return -1;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
        struct http_decoder_half_data *hf_data = NULL;
        if(HTTP_REQUEST == msg->flow_type)
        {
            hf_data = msg->ref_queue->array[msg->queue_index].req_data;
        }
        else
        {
            hf_data = msg->ref_queue->array[msg->queue_index].res_data;
        }
        return http_half_data_get_transaction_seq(hf_data);
    }
#ifdef __cplusplus
}
#endif