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

#ifdef __cplusplus
extern "C"
{
#endif

#include "mempool/nmx_palloc.h"
#include "stellar/utils.h"
#include "http_decoder.h"
#include "http_decoder_result_queue.h"


#define MEMPOOL_CALLOC(pool, type, number) ((type *)nmx_pcalloc(pool, sizeof(type) * number))
#define MEMPOOL_REALLOC(pool)
#define MEMPOOL_FREE(pool, p) nmx_pfree(pool, p)

#ifdef ENABLE_MEMPOOL

#define HD_CALLOC(pool, type, number) MEMPOOL_CALLOC(pool, number, type)
#define HD_FREE(pool, p) MEMPOOL_FREE(pool, p)

#else

#define HD_CALLOC(pool, type, number) CALLOC(type, number)
#define HD_FREE(pool, p) FREE(p)

#endif


struct http_message;

struct http_message *
http_message_new(enum http_message_type type,
                 struct http_decoder_result_queue *queue,
                 int queue_index);

#ifdef __cplusplus
}
#endif

#endif