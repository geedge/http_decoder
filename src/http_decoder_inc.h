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

struct http_decoder_env
{
    int plugin_id;
    int httpd_msg_topic_id;
    int ex_data_idx;
    struct stellar *st;
    struct http_decoder_config hd_cfg;
    struct http_decoder_stat hd_stat;
};

struct http_message;

struct http_message *http_message_new(enum http_message_type type, struct http_decoder_result_queue *queue,
                                    int queue_index, unsigned char flow_type);

#ifdef __cplusplus
}
#endif

#endif