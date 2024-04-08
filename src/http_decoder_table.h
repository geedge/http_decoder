/*
**********************************************************************************************
*	File: http_decoder_table.h
*   Description:
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/


#ifndef _HTTP_DECODER_TABLE_H_
#define _HTTP_DECODER_TABLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "http_decoder.h"
#include "http_decoder_inc.h"
#include "http_decoder_string.h"

enum http_item {
    HTTP_ITEM_URI = 0x01,
    HTTP_ITEM_STATUS = 0x02,
    HTTP_ITEM_METHOD = 0x03,
    HTTP_ITEM_VERSION = 0x04,
    HTTP_ITEM_HDRKEY = 0x05,
    HTTP_ITEM_HDRVAL = 0x06,
    HTTP_ITEM_BODY = 0x07,
};

struct http_decoder_table;
struct http_decoder_table *http_decoder_table_new(nmx_pool_t *mempool);

void http_decoder_table_free(struct http_decoder_table *table);

enum string_state
http_decoder_table_state(struct http_decoder_table *table, enum http_item type);

void http_decoder_table_refer(struct http_decoder_table *table, enum http_item type,
                              const char *at, size_t len);

void http_decoder_table_cache(struct http_decoder_table *table, enum http_item type);

void http_decoder_table_commit(struct http_decoder_table *table, enum http_item type);

void http_decoder_table_reset(struct http_decoder_table *table, enum http_item type);

void http_decoder_table_reinit(struct http_decoder_table *table);

void http_decoder_table_dump(struct http_decoder_table *table);

int http_decoder_table_get_uri(struct http_decoder_table *table, struct hstring *out);

int http_decoder_table_get_method(struct http_decoder_table *table, struct hstring *out);

int http_decoder_table_get_status(struct http_decoder_table *table, struct hstring *out);

int http_decoder_table_get_version(struct http_decoder_table *table, struct hstring *out);

int http_decoder_table_get_body(struct http_decoder_table *table, struct hstring *out);

int http_decoder_table_get_header(struct http_decoder_table *table,
                                  struct hstring *key,
                                  struct http_header *hdr_array,
                                  size_t array_size);

int http_decoder_table_iter_header(struct http_decoder_table *table,
                                   struct http_header *hdr);

/**
 * @brief Is there a parsed header
 * 
 * @retval yes(1) no(0)
*/
int http_decoder_table_has_parsed_header(struct http_decoder_table *table);

/**
 * @brief If headers have been parsed completely
 *
 * @retval yes(1) no(0)
 */
int http_decoder_table_header_complete(struct http_decoder_table *table);

/**
 * @brief set flag for headers parsed completely
*/
void http_decoder_table_set_header_complete(struct http_decoder_table *table);

void http_decoder_table_reset_header_complete(struct http_decoder_table *table);

#ifdef __cplusplus
}
#endif

#endif