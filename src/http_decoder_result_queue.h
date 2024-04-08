/*
**********************************************************************************************
*	File: http_decoder_result_queue.h
*   Description: 
*	Authors: Liuwentan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-15
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/


#ifndef _HTTP_DECODER_RESULT_QUEUE_H_
#define _HTTP_DECODER_RESULT_QUEUE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "mempool/nmx_palloc.h"
#include "http_decoder_half.h"

struct http_decoder_result {
    struct http_decoder_half_data *req_data;
    struct http_decoder_half_data *res_data;
};

struct http_decoder_result_queue {
	size_t req_index;
	size_t res_index;
	size_t queue_size;
	struct http_decoder_result *array;
};

struct http_decoder_result_queue *
http_decoder_result_queue_new(nmx_pool_t *mempool, size_t queue_size);

void http_decoder_result_queue_free(nmx_pool_t *mempool,
                                    struct http_decoder_result_queue *queue);

void http_decoder_result_queue_inc_req_index(struct http_decoder_result_queue *queue);

void http_decoder_result_queue_inc_res_index(struct http_decoder_result_queue *queue);

size_t http_decoder_result_queue_req_index(struct http_decoder_result_queue *queue);

size_t http_decoder_result_queue_res_index(struct http_decoder_result_queue *queue);

struct http_decoder_half_data *
http_decoder_result_queue_pop_req(struct http_decoder_result_queue *queue);

struct http_decoder_half_data *
http_decoder_result_queue_pop_res(struct http_decoder_result_queue *queue);

int http_decoder_result_queue_push_req(struct http_decoder_result_queue *queue,
                                       struct http_decoder_half_data *req_data);

int http_decoder_result_queue_push_res(struct http_decoder_result_queue *queue,
                                       struct http_decoder_half_data *res_data);

struct http_decoder_half_data *
http_decoder_result_queue_peek_req(struct http_decoder_result_queue *queue);

struct http_decoder_half_data *
http_decoder_result_queue_peek_res(struct http_decoder_result_queue *queue);

#ifdef __cplusplus
}
#endif

#endif