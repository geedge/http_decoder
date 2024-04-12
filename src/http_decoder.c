/*
**********************************************************************************************
*	File: http_decoder.c
*   Description:
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-10
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "toml/toml.h"
#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/session_exdata.h"
#include "http_decoder.h"
#include "http_decoder_half.h"
#include "http_decoder_table.h"
#include "http_decoder_result_queue.h"
#include "llhttp.h"
#include "http_decoder_inc.h"
#include "fieldstat/fieldstat_easy.h"

#define HTTP_IDENTIFY_LEN 16
#define HD_RESULT_QUEUE_LEN 16

#define DEFAULT_STAT_OUTPUT_INTERVAL 1
#define DEFAULT_STAT_INTERVAL_PKTS 1000
#define DEFAULT_MEMPOOL_SIZE (32 * 1024)

const char *g_hd_cfg_path = "./etc/http/http_decoder.toml";
const char *http_decoder_topic = "HTTP_DECODER_MESSAGE";
const char *fs_file_name = "http_decoder.fs";

struct http_decoder_config
{
    int decompress_switch;
    int stat_interval_pkts; // call fieldstat_incrby every stat_interval_pkts
    int stat_output_interval;
    size_t result_queue_len; // per session result queue length
    size_t mempool_size;     // per session mempool size
};

/**
 * NOTE: http_message don't have the ownership of data
 */
struct http_message
{
    enum http_message_type type;
    struct http_decoder_result_queue *ref_queue;
    size_t queue_index;
};

struct http_decoder
{
    struct http_decoder_half *c2s_half;
    struct http_decoder_half *s2c_half;
};

struct http_decoder_exdata
{
    struct http_decoder_result_queue *queue;
    struct http_decoder *decoder;
    nmx_pool_t *mempool;
};

struct http_decoder_stat
{
    long long incoming_bytes;
    long long incoming_pkts;
    long long incoming_trans;
    long long err_pkts;
    int counter;
};

struct http_decoder_context
{
    int plugin_id;
    int topic_id;
    int ex_data_idx;
    int fs_incoming_bytes_id;
    int fs_incoming_pkts_id;
    int fs_incoming_trans_id;
    int fs_err_pkts_id;
    struct stellar *st;
    struct fieldstat_easy *fse;
    struct http_decoder_config hd_cfg;
};

__thread struct http_decoder_stat _th_stat;

struct http_message *
http_message_new(enum http_message_type type,
                 struct http_decoder_result_queue *queue,
                 int queue_index)
{
    struct http_message *msg = CALLOC(struct http_message, 1);

    msg->type = type;
    msg->ref_queue = queue;
    msg->queue_index = queue_index;

    return msg;
}

static void http_message_free(void *http_msg, void *cb_arg)
{
    if (NULL == http_msg)
    {
        return;
    }

    FREE(http_msg);
}

static void http_event_handler(enum http_event event,
                               struct http_decoder_half_data **data,
                               struct http_event_context *ev_ctx)
{
    assert(ev_ctx);

    size_t queue_idx = 0;
    nmx_pool_t *mempool = ev_ctx->ref_mempool;
    struct http_decoder_result_queue *queue = ev_ctx->ref_queue;
    struct http_message *msg = NULL;
    struct http_decoder_half_data *half_data = NULL;
    int ret = 0;

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
        break;
    case HTTP_EVENT_REQ_LINE:
        queue_idx = http_decoder_result_queue_req_index(queue);
        msg = http_message_new(HTTP_MESSAGE_REQ_LINE, queue, queue_idx);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_HDR_END:
    {
        int build_url_final = http_decoder_join_url_finally(ev_ctx, http_decoder_result_queue_peek_req(queue), mempool);
        ret = http_decoder_half_data_has_parsed_header(*data);
        if (0 == ret && 0 == build_url_final)
        {
            break;
        }
        queue_idx = http_decoder_result_queue_req_index(queue);
        msg = http_message_new(HTTP_MESSAGE_REQ_HEADER, queue, queue_idx);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
    }
    break;
    case HTTP_EVENT_REQ_BODY_BEGIN:
        break;
    case HTTP_EVENT_REQ_BODY_DATA:
        queue_idx = http_decoder_result_queue_req_index(queue);
        msg = http_message_new(HTTP_MESSAGE_REQ_BODY, queue, queue_idx);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_REQ_BODY_END:
        break;
    case HTTP_EVENT_REQ_END:
        http_decoder_result_queue_inc_req_index(queue);
        half_data = http_decoder_result_queue_pop_req(queue);
        if (half_data != NULL)
        {
            http_decoder_half_data_free(mempool, half_data);
            half_data = NULL;
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
        *data = half_data;
        break;
    case HTTP_EVENT_RES_LINE:
        queue_idx = http_decoder_result_queue_res_index(queue);
        msg = http_message_new(HTTP_MESSAGE_RES_LINE, queue, queue_idx);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_HDR:
        break;
    case HTTP_EVENT_RES_HDR_END:
        ret = http_decoder_half_data_has_parsed_header(*data);
        if (0 == ret)
        {
            break;
        }

        queue_idx = http_decoder_result_queue_res_index(queue);
        msg = http_message_new(HTTP_MESSAGE_RES_HEADER, queue, queue_idx);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_BODY_BEGIN:
        break;
    case HTTP_EVENT_RES_BODY_DATA:
        queue_idx = http_decoder_result_queue_res_index(queue);
        msg = http_message_new(HTTP_MESSAGE_RES_BODY, queue, queue_idx);
        session_mq_publish_message(ev_ctx->ref_session, ev_ctx->topic_id, msg);
        break;
    case HTTP_EVENT_RES_BODY_END:
        break;
    case HTTP_EVENT_RES_END:
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
}

static struct http_decoder *
http_decoder_new(nmx_pool_t *mempool, http_event_cb *ev_cb,
                 int decompress_switch)
{
    struct http_decoder *decoder = MEMPOOL_CALLOC(mempool, struct http_decoder, 1);
    assert(decoder);

    decoder->c2s_half = http_decoder_half_new(mempool, ev_cb, HTTP_REQUEST,
                                              decompress_switch);
    decoder->s2c_half = http_decoder_half_new(mempool, ev_cb, HTTP_RESPONSE,
                                              decompress_switch);

    return decoder;
}

static void
http_decoder_free(nmx_pool_t *mempool, struct http_decoder *decoder)
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

static struct http_decoder_exdata *
http_decoder_exdata_new(size_t mempool_size, size_t queue_size,
                        int decompress_switch)
{
    struct http_decoder_exdata *ex_data = CALLOC(struct http_decoder_exdata, 1);

    ex_data->mempool = nmx_create_pool(mempool_size);
    ex_data->decoder = http_decoder_new(ex_data->mempool, http_event_handler,
                                        decompress_switch);
    ex_data->queue = http_decoder_result_queue_new(ex_data->mempool, queue_size);

    return ex_data;
}

static void http_decoder_exdata_free(struct http_decoder_exdata *ex_data)
{
    if (NULL == ex_data)
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

static int
http_decoder_stat_init(struct http_decoder_context *ctx, int thread_num)
{
    ctx->fse =
        fieldstat_easy_new(thread_num, "http_decoder_statistics", NULL, 0);
    if (NULL == ctx->fse)
    {
        fprintf(stderr, "fieldstat_easy_new failed.");
        return -1;
    }

    ctx->fs_incoming_bytes_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_bytes");
    if (ctx->fs_incoming_bytes_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_bytes failed.");
        return -1;
    }

    ctx->fs_incoming_trans_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_trans");
    if (ctx->fs_incoming_trans_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_trans failed.");
        return -1;
    }

    ctx->fs_incoming_pkts_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_pkts");
    if (ctx->fs_incoming_pkts_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_pkts failed.");
        return -1;
    }

    ctx->fs_err_pkts_id = fieldstat_easy_register_counter(ctx->fse, "err_pkts");
    if (ctx->fs_err_pkts_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter err_pkts failed.");
        return -1;
    }

    int stat_output_interval = DEFAULT_STAT_OUTPUT_INTERVAL;
    if (ctx->hd_cfg.stat_output_interval > 0)
    {
        stat_output_interval = ctx->hd_cfg.stat_output_interval;
    }

    int ret = fieldstat_easy_enable_auto_output(ctx->fse, fs_file_name,
                                                stat_output_interval);
    if (ret < 0)
    {
        fprintf(stderr, "fieldstat_easy_enable_auto_output failed.");
        return -1;
    }

    sleep(1);

    return 0;
}

static void
http_decoder_stat_output(struct http_decoder_context *ctx, int thread_id)
{
    if (NULL == ctx || thread_id < 0)
    {
        return;
    }

    int stat_interval_pkts = DEFAULT_STAT_INTERVAL_PKTS;
    if (ctx->hd_cfg.stat_interval_pkts > 0)
    {
        stat_interval_pkts = ctx->hd_cfg.stat_interval_pkts;
    }

    if (_th_stat.counter >= stat_interval_pkts)
    {
        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_incoming_bytes_id, NULL, 0,
                                      _th_stat.incoming_bytes);

        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_incoming_pkts_id, NULL, 0,
                                      _th_stat.incoming_pkts);

        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_incoming_trans_id, NULL, 0,
                                      _th_stat.incoming_trans);

        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_err_pkts_id, NULL, 0,
                                      _th_stat.err_pkts);

        _th_stat.counter = 0;
        _th_stat.err_pkts = 0;
        _th_stat.incoming_bytes = 0;
        _th_stat.incoming_pkts = 0;
        _th_stat.incoming_trans = 0;
    }
}

int http_decoder_entry(struct session *sess, int events,
                       const struct packet *pkt, void *cb_arg)
{
    struct http_decoder_context *ctx = (struct http_decoder_context *)cb_arg;
    size_t payload_len = 0;
    uint64_t inner_flag = 0;

    int ret = session_is_inner_most(sess, &inner_flag);
    if (0 == ret)
    {
        return 0;
    }

    struct http_decoder_exdata *ex_data =
        session_get_ex_data(sess, ctx->ex_data_idx);

    if (events & SESS_EV_CLOSING)
    {
        if (ex_data != NULL)
        {
            http_decoder_exdata_free(ex_data);
            session_set_ex_data(sess, ctx->ex_data_idx, NULL);
        }

        return 0;
    }

    const char *payload = session_get0_current_payload(sess, &payload_len);

    if (events & SESS_EV_OPENING)
    {
        assert(ex_data == NULL);

        // If not http, ignore this session
        if (payload_len > 0)
        {
            size_t http_identify_len = payload_len > HTTP_IDENTIFY_LEN
                                           ? HTTP_IDENTIFY_LEN
                                           : payload_len;

            ret = http_protocol_identify(payload, http_identify_len);
            if (ret < 0)
            {
                // ignore this session's event
                struct session_event *s_event =
                    session_get_intrinsic_event(sess, ctx->plugin_id);

                session_event_assign(s_event, ctx->st, sess, 0,
                                     http_decoder_entry, ctx);
                return 0;
            }
        }

        ex_data = http_decoder_exdata_new(ctx->hd_cfg.mempool_size,
                                          ctx->hd_cfg.result_queue_len,
                                          ctx->hd_cfg.decompress_switch);
        session_set_ex_data(sess, ctx->ex_data_idx, ex_data);
    }

    if (0 == payload_len || NULL == ex_data)
    {
        return 0;
    }

    int dir = packet_get_direction(pkt);
    if (dir < 0)
    {
        return -1;
    }

    int thread_id = session_get_current_thread_id(sess);
    struct http_decoder_half *cur_half = NULL;

    if (dir == PACKET_DIRECTION_C2S)
    {
        cur_half = ex_data->decoder->c2s_half;
    }
    else
    {
        cur_half = ex_data->decoder->s2c_half;
    }

    http_decoder_half_reinit(cur_half, ctx->topic_id, ex_data->queue,
                             ex_data->mempool, sess);
    ret = http_decoder_half_parse(cur_half, payload, payload_len);
    if (ret < 0)
    {
        _th_stat.err_pkts += 1;
    }

    _th_stat.incoming_bytes += payload_len;
    _th_stat.incoming_pkts += 1;
    _th_stat.incoming_trans += http_decoder_half_trans_count(cur_half);
    _th_stat.counter++;

    http_decoder_stat_output(ctx, thread_id);

    return 0;
}

static void _http_decoder_context_free(struct http_decoder_context *ctx)
{
    if (NULL == ctx)
    {
        return;
    }

    if (ctx->fse != NULL)
    {
        fieldstat_easy_free(ctx->fse);
        ctx->fse = NULL;
    }

    if (ctx->topic_id >= 0)
    {
        session_mq_destroy_topic(ctx->st, ctx->topic_id);
        ctx->topic_id = -1;
    }

    FREE(ctx);
}

static void http_decoder_ex_data_free(struct session *s, int idx,
                                      void *ex_data, void *arg)
{
    if (NULL == ex_data)
    {
        return;
    }

    struct http_decoder_exdata *exdata = (struct http_decoder_exdata *)ex_data;
    http_decoder_exdata_free(exdata);
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
        ret = -1;
        goto next;
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

next:
    toml_free(root);
    return ret;
}

void *http_decoder_init(struct stellar *st)
{
    int plugin_id = -1;
    int topic_id = -1;
    int thread_num = 0;

    struct http_decoder_context *ctx = CALLOC(struct http_decoder_context, 1);

    int ret = load_http_decoder_config(g_hd_cfg_path, &ctx->hd_cfg);
    if (ret < 0)
    {
        goto failed;
    }

    ctx->st = st;
    ctx->ex_data_idx = stellar_session_get_ex_new_index(st, "HTTP_DECODER",
                                                        http_decoder_ex_data_free,
                                                        NULL);

    plugin_id = stellar_plugin_register(st, SESS_EV_TCP | SESS_EV_CLOSING,
                                        http_decoder_entry, ctx);
    if (plugin_id < 0)
    {
        goto failed;
    }

    ctx->plugin_id = plugin_id;

    topic_id = session_mq_get_topic_id(st, http_decoder_topic);
    if (topic_id < 0)
    {
        topic_id = session_mq_create_topic(st, http_decoder_topic,
                                           http_message_free, NULL);
    }

    ctx->topic_id = topic_id;

    thread_num = stellar_get_worker_thread_num(st);

    if (http_decoder_stat_init(ctx, thread_num) < 0)
    {
        goto failed;
    }

    // printf("http_decoder_init: ex_data_idx:%d, plugin_id:%d, topic_id:%d\n",
    //        ctx->ex_data_idx, ctx->plugin_id, ctx->topic_id);
    return ctx;

failed:
    _http_decoder_context_free(ctx);
    return NULL;
}

void http_decoder_exit(void *decoder_ctx)
{
    if (NULL == decoder_ctx)
    {
        return;
    }

    struct http_decoder_context *ctx =
        (struct http_decoder_context *)decoder_ctx;

    _http_decoder_context_free(ctx);
}

enum http_message_type http_message_type(struct http_message *msg)
{
    if (NULL == msg)
    {
        return HTTP_MESSAGE_MAX;
    }

    return msg->type;
}

int http_message_get_request_line(struct http_message *msg,
                                  struct http_request_line *line)
{
    if (NULL == msg || msg->type != HTTP_MESSAGE_REQ_LINE ||
        NULL == line)
    {
        return -1;
    }

    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

    struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;

    return http_decoder_half_data_get_request_line(req_data, line);
}

int http_message_get_response_line(struct http_message *msg,
                                   struct http_response_line *line)
{
    if (NULL == msg || msg->type != HTTP_MESSAGE_RES_LINE ||
        NULL == line)
    {
        return -1;
    }

    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

    struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;

    return http_decoder_half_data_get_response_line(res_data, line);
}

static int http_msg_get_request_header(struct http_message *msg, struct hstring *key,
                                       struct http_header *hdr_result)
{
    struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_header(req_data, key, hdr_result);
}

static int http_msg_get_response_header(struct http_message *msg, struct hstring *key,
                                        struct http_header *hdr_result)
{
    struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_header(res_data, key, hdr_result);
}

int http_message_get_header(struct http_message *msg, struct hstring *key,
                            struct http_header *hdr_result)
{
    if (NULL == msg || NULL == key || NULL == hdr_result)
    {
        return -1;
    }
    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
    if (HTTP_MESSAGE_REQ_HEADER == msg->type)
    {
        return http_msg_get_request_header(msg, key, hdr_result);
    }
    else if (HTTP_MESSAGE_RES_HEADER == msg->type)
    {
        return http_msg_get_response_header(msg, key, hdr_result);
    }

    return -1;
}

static int http_msg_request_header_next(struct http_message *msg,
                                        struct http_header *hdr)
{
    struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_iter_header(req_data, hdr);
}

static int http_msg_response_header_next(struct http_message *msg,
                                         struct http_header *hdr)
{
    struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_iter_header(res_data, hdr);
}

int http_message_header_next(struct http_message *msg,
                             struct http_header *header)
{
    if (NULL == msg || NULL == header)
    {
        return -1;
    }
    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
    if (HTTP_MESSAGE_REQ_HEADER == msg->type)
    {
        return http_msg_request_header_next(msg, header);
    }
    else if (HTTP_MESSAGE_RES_HEADER == msg->type)
    {
        return http_msg_response_header_next(msg, header);
    }

    return -1;
}

int http_message_reset_header_iter(struct http_message *msg)
{
    if (NULL == msg)
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

static int http_msg_get_request_raw_body(struct http_message *msg,
                                         struct hstring *body)
{
    struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_raw_body(req_data, body);
}

static int http_msg_get_response_raw_body(struct http_message *msg,
                                          struct hstring *body)
{
    struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_raw_body(res_data, body);
}

int http_message_get_raw_body(struct http_message *msg,
                              struct hstring *body)
{
    if (NULL == msg || NULL == body)
    {
        return -1;
    }
    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

    if (HTTP_MESSAGE_REQ_BODY == msg->type)
    {
        return http_msg_get_request_raw_body(msg, body);
    }
    else if (HTTP_MESSAGE_RES_BODY == msg->type)
    {
        return http_msg_get_response_raw_body(msg, body);
    }

    return -1;
}

int http_msg_get_request_decompress_body(struct http_message *msg,
                                         struct hstring *body)
{
    struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;
    return http_decoder_half_data_get_decompress_body(req_data, body);
}

int http_msg_get_response_decompress_body(struct http_message *msg,
                                          struct hstring *body)
{
    struct http_decoder_half_data *res_data =
        msg->ref_queue->array[msg->queue_index].res_data;
    return http_decoder_half_data_get_decompress_body(res_data, body);
}

int http_message_get_decompress_body(struct http_message *msg,
                                     struct hstring *body)
{
    if (NULL == msg || NULL == body)
    {
        return -1;
    }

    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

    if (HTTP_MESSAGE_REQ_BODY == msg->type)
    {
        return http_msg_get_request_decompress_body(msg, body);
    }
    else if (HTTP_MESSAGE_RES_BODY == msg->type)
    {
        return http_msg_get_response_decompress_body(msg, body);
    }

    return -1;
}

int http_message_get_url(struct http_message *msg, struct hstring *url)
{
    if (NULL == msg || NULL == url)
    {
        return -1;
    }

    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);

    struct http_decoder_half_data *req_data =
        msg->ref_queue->array[msg->queue_index].req_data;

    return http_half_data_get_url(req_data, url);
}

int http_message_get_transaction_seq(struct http_message *msg)
{
    if (NULL == msg)
    {
        return -1;
    }
    assert(msg->ref_queue);
    assert(msg->queue_index < HD_RESULT_QUEUE_LEN);
    struct http_decoder_half_data *hf_data;
    if (HTTP_MESSAGE_REQ_LINE == msg->type || HTTP_MESSAGE_REQ_HEADER == msg->type || HTTP_MESSAGE_REQ_BODY == msg->type)
    {
        hf_data = msg->ref_queue->array[msg->queue_index].req_data;
    }
    else
    {
        hf_data = msg->ref_queue->array[msg->queue_index].res_data;
    }
    return http_half_data_get_transaction_seq(hf_data);
    ;
}