/*
**********************************************************************************************
*	File: http_decoder_inc.h
*   Description: 
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-10
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#ifndef _HTTP_DECODER_INC_H_
#define _HTTP_DECODER_INC_H_

#include <cstddef>
#ifdef __cplusplus
extern "C"
{
#endif
#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/session_exdata.h"

#include "mempool/nmx_palloc.h"
#include "stellar/utils.h"
#include "http_decoder.h"
#include "http_decoder_result_queue.h"
#include "http_decoder_half.h"
#include "http_decoder_table.h"
#include "http_decoder_result_queue.h"
#include "http_decoder_utils.h"
#include "http_decoder_stat.h"
#include "http_decoder_tunnel.h"
#include "fieldstat/fieldstat_easy.h"
#include "toml/toml.h"

#ifndef likely
#define likely(x)    __builtin_expect((x), 1)
#endif
#ifndef unlikely
#define unlikely(x)  __builtin_expect((x), 0)
#endif

#define MEMPOOL_CALLOC(pool, type, number) ((type *)nmx_pcalloc(pool, sizeof(type) * number))
#define MEMPOOL_REALLOC(pool)
#define MEMPOOL_FREE(pool, p) nmx_pfree(pool, p)

#define ENABLE_MEMPOOL 1
#ifdef ENABLE_MEMPOOL
#define HD_CALLOC(pool, type, number) MEMPOOL_CALLOC(pool, number, type)
#define HD_FREE(pool, p) MEMPOOL_FREE(pool, p)
#else
#define HD_CALLOC(pool, type, number) CALLOC(type, number)
#define HD_FREE(pool, p) FREE(p)
#endif

#define HTTP_IDENTIFY_LEN 16
#define HD_RESULT_QUEUE_LEN 16

#define DEFAULT_STAT_OUTPUT_INTERVAL 1
#define DEFAULT_STAT_INTERVAL_PKTS 1000
#define DEFAULT_MEMPOOL_SIZE (32 * 1024)

#define HTTPD_CFG_FILE   "./etc/http/http_decoder.toml"
#define FILEDSTAT_OUTPUT_FILE   "./http_decoder.fs4"

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
    uint8_t flow_type;
    enum http_message_type type;
    size_t queue_index;
    struct http_decoder_result_queue *ref_queue;
    hstring tunnel_payload;
};

struct http_decoder
{
    struct http_decoder_half *c2s_half;
    struct http_decoder_half *s2c_half;
};

enum httpd_topic_index{
    HTTPD_TOPIC_TCP_STREAM_INDEX = 0,
    HTTPD_TOPIC_HTTP_MSG_INDEX,
    HTTPD_TOPIC_HTTP_TUNNEL_INDEX,
    HTTPD_TOPIC_INDEX_MAX,
};  

struct http_decoder_exdata
{
    int sub_topic_id; //tcp_stream
    int pub_topic_id; //http message or http tunnel msg
    struct http_decoder_result_queue *queue;
    struct http_decoder *decoder;
    nmx_pool_t *mempool;
    enum http_tunnel_state tunnel_state; 
    int in_tunnel_is_http;
};

// struct http_decoder_context{
//     int array_size;
//     struct http_decoder_exdata **exdata_array; //raw tcp stream for http msg;  http tunnel for inner http transaction. 
// };

struct http_topic_exdata_compose{
    enum httpd_topic_index index;
    const char *topic_name;
    on_session_msg_cb_func *on_msg_cb;
    session_msg_free_cb_func *msg_free_cb;
    const char *exdata_name;
    session_exdata_free *exdata_free_cb;
    int sub_topic_id; //as consumer 
    int exdata_id;
};

struct http_decoder_env
{
    struct stellar *st;
    int plugin_id;
    struct http_topic_exdata_compose topic_exdata_compose[HTTPD_TOPIC_INDEX_MAX];
    struct http_decoder_config hd_cfg;
    struct http_decoder_stat hd_stat;
};

struct http_message;

struct http_message *http_message_new(enum http_message_type type, struct http_decoder_result_queue *queue,
                                    int queue_index, unsigned char flow_type);
int http_topic_exdata_compose_get_index(const struct http_decoder_env *httpd_env, int by_topic_id);
#ifdef __cplusplus
}
#endif

#endif