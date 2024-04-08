/*
**********************************************************************************************
*	File: http_decoder_result_queue.c
*   Description: 
*	Authors: Liuwentan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-15
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#include <assert.h>

#include "stellar/utils.h"
#include "http_decoder_half.h"
#include "http_decoder_inc.h"
#include "http_decoder_result_queue.h"

struct http_decoder_result_queue *
http_decoder_result_queue_new(nmx_pool_t *mempool, size_t queue_size)
{
    struct http_decoder_result_queue *queue =
        MEMPOOL_CALLOC(mempool, struct http_decoder_result_queue, 1);
    assert(queue);

    queue->req_index = 0;
    queue->res_index = 0;
    queue->queue_size = queue_size;

    queue->array = MEMPOOL_CALLOC(mempool, struct http_decoder_result,
                                  queue->queue_size);
    assert(queue->array);

    return queue;
}

void http_decoder_result_queue_free(nmx_pool_t *mempool,
                                    struct http_decoder_result_queue *queue)
{
	if (NULL == queue) {
		return;
    }

    if (queue->array != NULL) {
        for (size_t i = 0; i < queue->queue_size; i++) {
            if (queue->array[i].req_data != NULL) {
            	http_decoder_half_data_free(mempool, queue->array[i].req_data);
				queue->array[i].req_data = NULL;
            }

            if (queue->array[i].res_data != NULL) {
            	http_decoder_half_data_free(mempool, queue->array[i].res_data);
				queue->array[i].res_data = NULL;
            }
        }
        
		MEMPOOL_FREE(mempool, queue->array);
    }

    MEMPOOL_FREE(mempool, queue);
}

void http_decoder_result_queue_inc_req_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->req_index++;
    queue->req_index = queue->req_index % queue->queue_size;
}

void http_decoder_result_queue_inc_res_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    queue->res_index++;
    queue->res_index = queue->res_index % queue->queue_size;
}

size_t http_decoder_result_queue_req_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    return queue->req_index;
}

size_t http_decoder_result_queue_res_index(struct http_decoder_result_queue *queue)
{
    assert(queue);

    return queue->res_index;
}

int http_decoder_result_queue_push_req(struct http_decoder_result_queue *queue,
                                       struct http_decoder_half_data *req_data)
{
    if (NULL == queue || NULL == req_data) {
        return -1;
    }

    assert(queue->array[queue->req_index].req_data == NULL);
    if (queue->array[queue->req_index].req_data != NULL) {
        return -1;
    }

    queue->array[queue->req_index].req_data = req_data;
    return 0;
}

int http_decoder_result_queue_push_res(struct http_decoder_result_queue *queue,
                                       struct http_decoder_half_data *res_data)
{
    if (NULL == queue || NULL == res_data) {
        return -1;
    }

    assert(queue->array[queue->res_index].res_data == NULL);
    if (queue->array[queue->res_index].res_data != NULL) {
        return -1;
    }

    queue->array[queue->res_index].res_data = res_data;
    return 0;
}

struct http_decoder_half_data *
http_decoder_result_queue_pop_req(struct http_decoder_result_queue *queue)
{
    if (NULL == queue) {
        return NULL;
    }

    struct http_decoder_half_data *req_data =
        queue->array[queue->req_index].req_data;
    queue->array[queue->req_index].req_data = NULL;

    return req_data;
}

struct http_decoder_half_data *
http_decoder_result_queue_pop_res(struct http_decoder_result_queue *queue)
{
    if (NULL == queue) {
        return NULL;
    }

    struct http_decoder_half_data *res_data =
        queue->array[queue->res_index].res_data;
    queue->array[queue->res_index].res_data = NULL;

    return res_data;
}

struct http_decoder_half_data *
http_decoder_result_queue_peek_req(struct http_decoder_result_queue *queue)
{
    if (NULL == queue) {
        return NULL;
    }

    assert(queue->req_index < queue->queue_size);
    return queue->array[queue->req_index].req_data;
}

struct http_decoder_half_data *
http_decoder_result_queue_peek_res(struct http_decoder_result_queue *queue)
{
    if (NULL == queue) {
        return NULL;
    }

    assert(queue->res_index < queue->queue_size);
    return queue->array[queue->res_index].res_data;
}