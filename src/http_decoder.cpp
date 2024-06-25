#include "http_decoder.h"
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
    struct http_decoder_exdata *exdata = ev_ctx->ref_httpd_ctx;

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
        /* llhttp always call on_message_begin() even if llhttp_execute() error!!! */
        msg = http_message_new(HTTP_TRANSACTION_NEW, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);      
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TRANSACTION_NEW, 1);  
        break;
    case HTTP_EVENT_REQ_LINE:
        msg = http_message_new(HTTP_MESSAGE_REQ_LINE, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        if(httpd_tunnel_identify(PACKET_DIRECTION_C2S, half_data)){
            exdata->tunnel_state = HTTP_TUN_C2S_HDR_START;
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TUNNEL, 1); 
        }
        http_decoder_get_url(half_data, mempool);
        break;
    case HTTP_EVENT_REQ_HDR:
        msg = http_message_new(HTTP_MESSAGE_REQ_HEADER, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
    break;
    case HTTP_EVENT_REQ_HDR_END:
    {
        http_decoder_join_url_finally(ev_ctx, half_data, mempool);
        /*  maybe some parsed headers in buffer, but has not pushed to plugins yet */
        
        if(http_decoder_half_data_has_parsed_header(half_data)){
            msg = http_message_new(HTTP_MESSAGE_REQ_HEADER, queue, queue_idx, HTTP_REQUEST);
            session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        }
        http_half_data_update_commit_index(half_data);
        msg = http_message_new(HTTP_MESSAGE_REQ_HEADER_END, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);       

        int tot_c2s_headers = http_half_data_get_total_parsed_header_count(half_data); 
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_HEADERS_C2S, tot_c2s_headers);

        hstring tmp_url = {};
        http_half_data_get_url(half_data, &tmp_url);
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_URL_BYTES, tmp_url.iov_len);
    }
    break;
    case HTTP_EVENT_REQ_BODY_BEGIN:
        //do nothing, in BODY_DATA event
        break;
    case HTTP_EVENT_REQ_BODY_DATA:
        msg = http_message_new(HTTP_MESSAGE_REQ_BODY, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_END:
        msg = http_message_new(HTTP_MESSAGE_REQ_BODY_END, queue, queue_idx, HTTP_REQUEST);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        break;
    case HTTP_EVENT_REQ_END:
    {
        session_is_symmetric(ev_ctx->ref_session, &flow_flag);

        if(SESSION_SEEN_C2S_FLOW == flow_flag){
            msg = http_message_new(HTTP_TRANSACTION_FREE, queue, queue_idx, HTTP_REQUEST);
            session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TRANSACTION_FREE, 1);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_ASYMMETRY_TRANSACTION_C2S, 1);
        }
        if(httpd_is_tunnel_session(exdata)){
            if(SESSION_SEEN_C2S_FLOW == flow_flag){
                exdata->tunnel_state = HTTP_TUN_INNER_STARTING;
                exdata->pub_topic_id = httpd_env->topic_exdata_compose[HTTPD_TOPIC_HTTP_TUNNEL_INDEX].sub_topic_id;
            }else{
                exdata->tunnel_state = HTTP_TUN_C2S_END;
            }
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
                session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);    
            }
        }
        break;
    case HTTP_EVENT_RES_LINE:
        msg = http_message_new(HTTP_MESSAGE_RES_LINE, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        if(httpd_tunnel_identify(PACKET_DIRECTION_S2C, half_data)){
            exdata->tunnel_state = HTTP_TUN_S2C_START;
        }else{
            //connect response fail, reset tunnel_state
            exdata->tunnel_state = HTTP_TUN_NON;
        }
        break;
    case HTTP_EVENT_RES_HDR:
        msg = http_message_new(HTTP_MESSAGE_RES_HEADER, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        break;
    case HTTP_EVENT_RES_HDR_END:
    {
        /*  maybe some header in table buffer but has not pushed to plugins */
        half_data = http_decoder_result_queue_peek_res(queue);
        if(http_decoder_half_data_has_parsed_header(half_data)){
            msg = http_message_new(HTTP_MESSAGE_RES_HEADER, queue, queue_idx, HTTP_RESPONSE);
            session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        }
        http_half_data_update_commit_index(half_data);
        msg = http_message_new(HTTP_MESSAGE_RES_HEADER_END, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);        

        int tot_s2c_headers = http_half_data_get_total_parsed_header_count(half_data); 
        http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_HEADERS_S2C, tot_s2c_headers);
        
        if(httpd_is_tunnel_session(exdata)){
            exdata->tunnel_state = HTTP_TUN_INNER_STARTING;
            http_half_pre_context_free(ev_ctx->ref_session, exdata);
            exdata->pub_topic_id = httpd_env->topic_exdata_compose[HTTPD_TOPIC_HTTP_TUNNEL_INDEX].sub_topic_id;
        } 
    }
        break;
    case HTTP_EVENT_RES_BODY_BEGIN:
        break;
    case HTTP_EVENT_RES_BODY_DATA:
        msg = http_message_new(HTTP_MESSAGE_RES_BODY, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
        break;
    case HTTP_EVENT_RES_BODY_END:
        msg = http_message_new(HTTP_MESSAGE_RES_BODY_END, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);    
        break;
    case HTTP_EVENT_RES_END:
        msg = http_message_new(HTTP_TRANSACTION_FREE, queue, queue_idx, HTTP_RESPONSE);
        session_mq_publish_message(ev_ctx->ref_session, exdata->pub_topic_id, msg);
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

static struct http_decoder *http_decoder_new(struct http_decoder_exdata *hd_ctx, nmx_pool_t *mempool, 
                    http_event_cb *ev_cb, int decompress_switch, struct http_decoder_env *httpd_env,
                    long long req_start_seq, long long res_start_seq)
{
    struct http_decoder *decoder = MEMPOOL_CALLOC(mempool, struct http_decoder, 1);
    assert(decoder);
    decoder->c2s_half = http_decoder_half_new(hd_ctx, mempool, ev_cb, HTTP_REQUEST,  decompress_switch, httpd_env, req_start_seq);
    decoder->s2c_half = http_decoder_half_new(hd_ctx, mempool, ev_cb, HTTP_RESPONSE, decompress_switch, httpd_env, res_start_seq);
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
                                        int decompress_switch, struct http_decoder_env *httpd_env,
                                        long long req_start_seq, long long res_start_seq)
{
    struct http_decoder_exdata *hd_ctx = CALLOC(struct http_decoder_exdata, 1);
    hd_ctx->mempool = nmx_create_pool(mempool_size);
    hd_ctx->decoder = http_decoder_new(hd_ctx, hd_ctx->mempool, http_event_handler, decompress_switch, 
                                        httpd_env, req_start_seq, res_start_seq);
    hd_ctx->queue = http_decoder_result_queue_new(hd_ctx->mempool, queue_size);
    return hd_ctx;
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
    if(ex_data->mempool){
        nmx_destroy_pool(ex_data->mempool);
    }
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
    return 1;
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

    for(int i = 0; i < HTTPD_TOPIC_INDEX_MAX; i++){
        if(env->topic_exdata_compose[i].msg_free_cb){
            stellar_session_mq_destroy_topic(env->st, env->topic_exdata_compose[i].sub_topic_id);
        }
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

static int http_msg_get_request_header(const struct http_message *msg, const hstring *key,
                                        struct http_header *hdr_result)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_header(req_data, key, hdr_result);
}

static int http_msg_get_response_header(const struct http_message *msg, const hstring *key,
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
                                            hstring *body)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_raw_body(req_data, body);
}

static int http_msg_get_response_raw_body(const struct http_message *msg,
                                            hstring *body)
{
    const struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_raw_body(res_data, body);
}

static int http_msg_get_request_decompress_body(const struct http_message *msg,
                                            hstring *body)
{
    const struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_decompress_body(req_data, body);
}

static int http_msg_get_response_decompress_body(const struct http_message *msg,
                                            hstring *body)
{
    const struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_decompress_body(res_data, body);
}

static struct http_decoder_exdata *httpd_session_exdata_new(struct session *sess, struct http_decoder_env *httpd_env, 
                                                            long long req_start_seq, long long res_start_seq)
{
    struct http_decoder_exdata *exdata = http_decoder_exdata_new(httpd_env->hd_cfg.mempool_size,
                                                                      httpd_env->hd_cfg.result_queue_len,
                                                                      httpd_env->hd_cfg.decompress_switch,
                                                                      httpd_env,req_start_seq,res_start_seq);
    // exdata->sub_topic_id = sub_topic_id;
    int thread_id = stellar_get_current_thread_id(httpd_env->st);
    http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_SESSION_NEW, 1);  
    return exdata;
}

#ifdef __cplusplus
extern "C"
{
#endif

    void httpd_ex_data_free_cb(struct session *s, int idx, void *ex_data, void *arg)
    {
        if (NULL == ex_data)
        {
            return;
        }
        struct http_decoder_exdata *exdata = (struct http_decoder_exdata *)ex_data;
        http_decoder_exdata_free(exdata);
    }

    void *httpd_session_ctx_new_cb(struct session *sess, void *plugin_env)
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
                return (void *)HTTP_CTX_NOT_HTTP;
            }
        }

        return (void *)HTTP_CTX_IS_HTTP;
    }

    void httpd_session_ctx_free_cb(struct session *sess, void *session_ctx, void *plugin_env)
    {
        if(NULL == plugin_env || NULL == session_ctx){
            return;
        }
        if(strncmp((const char *)session_ctx, HTTP_CTX_NOT_HTTP, strlen(HTTP_CTX_NOT_HTTP)) == 0){
            return;
        }
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        int thread_id = session_get_current_thread_id(sess);
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

    static void http_decoder_execute(struct session *sess, struct http_decoder_env *httpd_env, http_decoder_exdata *exdata)
    {
        size_t payload_len;
        const char *payload = session_get0_current_payload(sess, &payload_len);
        if (unlikely(0 == payload_len || NULL == payload))
        {
            return;
        }

        if(httpd_in_tunnel_transmitting(exdata)){
            http_decoder_push_tunnel_data(sess, exdata, httpd_tunnel_state_to_msg(exdata));
            httpd_tunnel_state_update(exdata);
            return;
        }

        int thread_id = session_get_current_thread_id(sess);
        struct http_decoder_half *cur_half = NULL;        
        int sess_dir = packet_get_direction(session_get0_current_packet(sess));
        if (PACKET_DIRECTION_C2S == sess_dir)
        {
            cur_half = exdata->decoder->c2s_half;
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_BYTES_C2S, payload_len);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TCP_SEG_C2S, 1);
        }
        else
        {
            cur_half = exdata->decoder->s2c_half;
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_BYTES_S2C, payload_len);
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_TCP_SEG_S2C, 1);            
        }

        http_decoder_half_reinit(cur_half, exdata->queue, exdata->mempool, sess);
        int ret = http_decoder_half_parse(cur_half, payload, payload_len);
        if (ret < 0)
        {
            http_decoder_stat_update(&httpd_env->hd_stat, thread_id, HTTPD_STAT_PARSE_ERR, 1);
            stellar_session_plugin_dettach_current_session(sess);
        }
    }

    void http_decoder_tunnel_msg_cb(struct session *sess, int topic_id, const void *tmsg, void *per_session_ctx, void *plugin_env)
    {
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        struct http_decoder_exdata *exdata = (struct http_decoder_exdata *)session_exdata_get(sess, httpd_env->topic_exdata_compose[HTTPD_TOPIC_HTTP_TUNNEL_INDEX].exdata_id);
        enum http_tunnel_message_type tmsg_type = http_tunnel_message_type_get((const struct http_tunnel_message *)tmsg);
        switch (tmsg_type)
        {
            case HTTP_TUNNEL_OPENING:
            {
                if(NULL != exdata){
                    //not support nested http tunnel 
                    session_mq_ignore_message(sess, topic_id, httpd_env->plugin_id);
                    return;
                }
                size_t payload_len;
                const char *payload = session_get0_current_payload(sess, &payload_len);
                size_t http_identify_len = payload_len > HTTP_IDENTIFY_LEN ? HTTP_IDENTIFY_LEN : payload_len;
                int is_http = http_protocol_identify(payload, http_identify_len);
                if(is_http){
                    long long max_req_seq = 0, max_res_seq = 0;
                    struct http_decoder_exdata *tcp_stream_exdata = (struct http_decoder_exdata *)session_exdata_get(sess, httpd_env->topic_exdata_compose[HTTPD_TOPIC_TCP_STREAM_INDEX].exdata_id);
                    http_half_get_max_transaction_seq(tcp_stream_exdata, &max_req_seq, &max_res_seq);
                    exdata = httpd_session_exdata_new(sess, httpd_env, max_req_seq, max_res_seq);
                    session_exdata_set(sess, httpd_env->topic_exdata_compose[HTTPD_TOPIC_HTTP_TUNNEL_INDEX].exdata_id, exdata);
                    exdata->pub_topic_id = httpd_env->topic_exdata_compose[HTTPD_TOPIC_HTTP_MSG_INDEX].sub_topic_id;
                    exdata->in_tunnel_is_http = 1;
                }else{
                    //inner tunnel is not http, do nothing, do not push this message again !!!
                    session_mq_ignore_message(sess, topic_id, httpd_env->plugin_id);
                    return;
                }
            }
            break;

            case HTTP_TUNNEL_ACTIVE:
                if(NULL == exdata){
                    session_mq_ignore_message(sess, topic_id, httpd_env->plugin_id);
                    http_decoder_stat_update(&httpd_env->hd_stat, session_get_current_thread_id(sess), HTTPD_STAT_PARSE_ERR, 1);  
                    return;
                }            
            break;

            case HTTP_TUNNEL_CLOSING:
                if(NULL == exdata){
                    http_decoder_stat_update(&httpd_env->hd_stat, session_get_current_thread_id(sess), HTTPD_STAT_PARSE_ERR, 1);  
                    return;
                }                
                if(exdata->in_tunnel_is_http){
                    http_half_pre_context_free(sess, exdata);
                }
                return;
            break;
            
            default:
                break;
        }
        if(exdata->in_tunnel_is_http){
            http_decoder_execute(sess, httpd_env, exdata);
        }
        return;
    }

    void http_decoder_tcp_stream_msg_cb(struct session *sess, int topic_id, const void *msg, void *nouse_session_ctx, void *plugin_env)
    {
        struct http_decoder_env *httpd_env = (struct http_decoder_env *)plugin_env;
        enum session_state sess_state = session_get_current_state(sess);
        struct http_decoder_exdata *exdata = (struct http_decoder_exdata *)session_exdata_get(sess, httpd_env->topic_exdata_compose[HTTPD_TOPIC_TCP_STREAM_INDEX].exdata_id);

        switch(sess_state){
            case SESSION_STATE_OPENING:
            {
                exdata = httpd_session_exdata_new(sess, httpd_env, 0, 0);
                exdata->pub_topic_id =  httpd_env->topic_exdata_compose[HTTPD_TOPIC_HTTP_MSG_INDEX].sub_topic_id;
                session_exdata_set(sess, httpd_env->topic_exdata_compose[HTTPD_TOPIC_TCP_STREAM_INDEX].exdata_id, exdata);
                //go on
            }
            break;

            case SESSION_STATE_ACTIVE:
            if(NULL == exdata){
                session_mq_ignore_message(sess, topic_id, httpd_env->plugin_id);
                http_decoder_stat_update(&httpd_env->hd_stat, session_get_current_thread_id(sess), HTTPD_STAT_PARSE_ERR, 1);  
                return;
            }
            //go on
            break;

            case SESSION_STATE_CLOSING:
            {
                if(NULL == exdata){
                    http_decoder_stat_update(&httpd_env->hd_stat, session_get_current_thread_id(sess), HTTPD_STAT_PARSE_ERR, 1);  
                    return;
                }                
                if(httpd_in_tunnel_transmitting(exdata)){
                    http_decoder_push_tunnel_data(sess, exdata, HTTP_TUNNEL_CLOSING);
                }else{
                    http_half_pre_context_free(sess, exdata);
                }
                return;
            }
            break;

            default:
                return;
            break;
        }

        http_decoder_execute(sess, httpd_env, exdata);
        
        return;
    }

static const struct http_topic_exdata_compose g_topic_exdata_compose[HTTPD_TOPIC_INDEX_MAX] = 
{
    {HTTPD_TOPIC_TCP_STREAM_INDEX,  TOPIC_TCP_STREAM, http_decoder_tcp_stream_msg_cb, NULL, "HTTP_DECODER_EXDATA_BASEON_TCP_STREAM", httpd_ex_data_free_cb, -1, -1},
    {HTTPD_TOPIC_HTTP_MSG_INDEX,    HTTP_DECODER_TOPIC, NULL, http_message_free, NULL, NULL, -1, -1},
    {HTTPD_TOPIC_HTTP_TUNNEL_INDEX, HTTP_DECODER_TUNNEL_TOPIC, http_decoder_tunnel_msg_cb, http_message_free, "HTTP_DECODER_EXDATA_BASEON_HTTP_TUNNEL",  httpd_ex_data_free_cb, -1, -1},
};

    static void http_decoder_topic_exdata_compose_init(struct http_decoder_env *httpd_env)
    {
        memcpy(httpd_env->topic_exdata_compose, g_topic_exdata_compose, sizeof(g_topic_exdata_compose));
        for(int i = 0; i < HTTPD_TOPIC_INDEX_MAX; i++){
            httpd_env->topic_exdata_compose[i].sub_topic_id = stellar_session_mq_get_topic_id_reliable(httpd_env->st, 
                                                                httpd_env->topic_exdata_compose[i].topic_name, 
                                                               httpd_env->topic_exdata_compose[i].msg_free_cb, 
                                                            NULL);
            assert(httpd_env->topic_exdata_compose[i].sub_topic_id >= 0);

            if(httpd_env->topic_exdata_compose[i].exdata_name){
                httpd_env->topic_exdata_compose[i].exdata_id = stellar_session_exdata_new_index(httpd_env->st, 
                                                                    httpd_env->topic_exdata_compose[i].exdata_name, 
                                                            httpd_env->topic_exdata_compose[i].exdata_free_cb, 
                                                                    NULL);
                assert(httpd_env->topic_exdata_compose[i].exdata_id >= 0);
            }

            if(httpd_env->topic_exdata_compose[i].on_msg_cb){
                stellar_session_mq_subscribe(httpd_env->st, httpd_env->topic_exdata_compose[i].sub_topic_id, 
                            httpd_env->topic_exdata_compose[i].on_msg_cb, httpd_env->plugin_id);
            }
        }
    }

    int http_topic_exdata_compose_get_index(const struct http_decoder_env *httpd_env, int by_topic_id)
    {
        for(int i = 0; i < HTTPD_TOPIC_INDEX_MAX; i++){
            if(httpd_env->topic_exdata_compose[i].sub_topic_id == by_topic_id){
                return i;
            }
        }
        assert(0);
        return -1;
    }

    void *http_decoder_init(struct stellar *st)
    {
        int thread_num = 0;

        struct http_decoder_env *httpd_env = CALLOC(struct http_decoder_env, 1);
        int ret = load_http_decoder_config(HTTPD_CFG_FILE, &httpd_env->hd_cfg);
        if (ret < 0)
        {
            goto failed;
        }
        httpd_env->st = st;
        httpd_env->plugin_id = stellar_session_plugin_register(st, httpd_session_ctx_new_cb,
                                                               httpd_session_ctx_free_cb, (void *)httpd_env);
        if (httpd_env->plugin_id < 0)
        {
            goto failed;
        }
        http_decoder_topic_exdata_compose_init(httpd_env);

        thread_num = stellar_get_worker_thread_num(st);
        assert(thread_num >= 1);
        if (http_decoder_stat_init( &httpd_env->hd_stat, thread_num, 
    httpd_env->hd_cfg.stat_interval_pkts, httpd_env->hd_cfg.stat_output_interval) < 0)
        {
            goto failed;
        }
        
        // printf("http decoder init succ, plugin id:%d \n", httpd_env->plugin_id);
        // for(int i = 0; i < HTTPD_TOPIC_INDEX_MAX; i++){
        //     printf("\ttopic_name:%s, topic_id:%d, ex_data_name:%s, exdata_id:%d\n", 
        //             httpd_env->topic_exdata_compose[i].topic_name, 
        //             httpd_env->topic_exdata_compose[i].sub_topic_id, 
        //             httpd_env->topic_exdata_compose[i].exdata_name,
        //             httpd_env->topic_exdata_compose[i].exdata_id);
        // }
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
                line->method.iov_base = NULL;
                line->uri.iov_base = NULL;
                line->version.iov_base = NULL;
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
                line->version.iov_base = NULL;
                line->status.iov_base = NULL;
            }
            return;
        }
        assert(msg->ref_queue);
        assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

        struct http_decoder_half_data *res_data =
            msg->ref_queue->array[msg->queue_index].res_data;

        http_decoder_half_data_get_response_line(res_data, line);
    }

    void http_message_get_header(const struct http_message *msg, const hstring *key,
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
            hdr_result->key.iov_base = NULL;
            hdr_result->val.iov_base = NULL;
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
            header->key.iov_base = NULL;
            header->val.iov_base = NULL;
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
                                  hstring *body)
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
            body->iov_base = NULL;
            body->iov_len = 0;
        }
        return;
    }

    void http_message_get_decompress_body(const struct http_message *msg,
                                         hstring *body)
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
            body->iov_base = NULL;
            body->iov_len = 0;
        }
        return;    
    }

    void http_message_get_url(const struct http_message *msg, hstring *url)
    {
        if (unlikely(NULL == msg))
        {
            if(url){
                url->iov_base = NULL;
                url->iov_len = 0;
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
            url->iov_base = NULL;
            url->iov_len = 0;
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