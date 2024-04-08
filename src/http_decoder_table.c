/*
**********************************************************************************************
*	File: http_decoder_table.c
*   Description:
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/


#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "http_decoder_table.h"
#include "http_decoder.h"
#include "http_decoder_string.h"
#include "stellar/utils.h"

#define INIT_HEADER_CNT 16
#define MAX_URI_CACHE_SIZE 2048
#define MAX_STATUS_CACHE_SIZE 32
#define MAX_METHOD_CACHE_SIZE 8
#define MAX_VERSION_CACHE_SIZE 4
#define MAX_HEADER_KEY_CACHE_SIZE 4096
#define MAX_HEADER_VALUE_CACHE_SIZE 4096

struct http_decoder_header {
    struct http_decoder_string key;
    struct http_decoder_string val;
};

struct http_decoder_table {
    struct http_decoder_string uri;
    struct http_decoder_string status;
    struct http_decoder_string method;
    struct http_decoder_string version;
    struct http_decoder_string body;

    nmx_pool_t *ref_mempool;
    int header_complete; // flag for all headers parsed completely
    size_t header_cnt;
    size_t header_index;
    size_t header_iter;
    struct http_decoder_header *headers;
};

static void http_decoder_table_init(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    http_decoder_string_init(&table->uri, MAX_URI_CACHE_SIZE);
    http_decoder_string_init(&table->status, MAX_STATUS_CACHE_SIZE);
    http_decoder_string_init(&table->method, MAX_METHOD_CACHE_SIZE);
    http_decoder_string_init(&table->version, MAX_METHOD_CACHE_SIZE);

    for (size_t i = 0; i < table->header_cnt; i++) {
        header = &table->headers[i];
        http_decoder_string_init(&header->key, MAX_HEADER_KEY_CACHE_SIZE);
        http_decoder_string_init(&header->val, MAX_HEADER_VALUE_CACHE_SIZE);
    }
        
    http_decoder_string_init(&table->body, 0);
}

struct http_decoder_table *http_decoder_table_new(nmx_pool_t *mempool)
{
    struct http_decoder_table *table =
        MEMPOOL_CALLOC(mempool, struct http_decoder_table, 1);
    assert(table);

    table->ref_mempool = mempool;
    table->header_cnt = INIT_HEADER_CNT;
    table->headers = MEMPOOL_CALLOC(mempool, struct http_decoder_header,
                                    table->header_cnt);

    http_decoder_table_init(table);

    return table;
}

void http_decoder_table_free(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    if (table->uri.cache.str != NULL) {
        FREE(table->uri.cache.str);
    }

    if (table->status.cache.str != NULL) {
        FREE(table->status.cache.str);
    }

    if (table->method.cache.str != NULL) {
        FREE(table->method.cache.str);
    }

    if (table->version.cache.str != NULL) {
        FREE(table->version.cache.str);
    }

    if (table->body.cache.str != NULL) {
        FREE(table->body.cache.str);
    }

    if (table->headers != NULL) {
        for (size_t i = 0; i < table->header_cnt; i++) {
            if (table->headers[i].key.cache.str != NULL) {
                FREE(table->headers[i].key.cache.str);
            }

            if (table->headers[i].val.cache.str != NULL) {
                FREE(table->headers[i].val.cache.str);
            }
        }

        MEMPOOL_FREE(table->ref_mempool, table->headers);
        table->headers = NULL;
    }

    MEMPOOL_FREE(table->ref_mempool, table);
}

enum string_state
http_decoder_table_state(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return STRING_STATE_INIT;
    }

    struct http_decoder_header *header = NULL;
    enum string_state state = STRING_STATE_INIT;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        state = http_decoder_string_state(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        state = http_decoder_string_state(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        state = http_decoder_string_state(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        state = http_decoder_string_state(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        state = http_decoder_string_state(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        state = http_decoder_string_state(&header->val);
        break;
    case HTTP_ITEM_BODY:
        state = http_decoder_string_state(&table->body);
        break;
    default:
        abort();
        break;
    }

    return state;
}

void http_decoder_table_refer(struct http_decoder_table *table, enum http_item type,
                              const char *at, size_t len)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_refer(&table->uri, at, len);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_refer(&table->status, at, len);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_refer(&table->method, at, len);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_refer(&table->version, at, len);
        break;
    case HTTP_ITEM_HDRKEY:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        http_decoder_string_refer(&header->key, at, len);
        break;
    case HTTP_ITEM_HDRVAL:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        http_decoder_string_refer(&header->val, at, len);
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_refer(&table->body, at, len);
        break;
    default:
        abort();
        break;
    }
}

void http_decoder_table_cache(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_cache(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_cache(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_cache(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_cache(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        http_decoder_string_cache(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        http_decoder_string_cache(&header->val);
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_cache(&table->body);
        break;
    default:
        abort();
        break;
    }
}

void http_decoder_table_commit(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return;
    }

    size_t i = 0;
    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_commit(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_commit(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_commit(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_commit(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        assert(table->header_index < table->header_cnt);
        header = &table->headers[table->header_index];
        http_decoder_string_commit(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        http_decoder_string_commit(&header->val);
        // inc index
        if ((table->header_index + 1) >= table->header_cnt) {
            struct http_decoder_header *old_headers = table->headers;
            table->headers =
                MEMPOOL_CALLOC(table->ref_mempool, struct http_decoder_header,
                               table->header_cnt * 2);
            table->header_cnt *= 2;

            for (i = 0; i <= table->header_index; i++) {
                table->headers[i] = old_headers[i];
            }

            MEMPOOL_FREE(table->ref_mempool, old_headers);

            for (i = table->header_index + 1; i < table->header_cnt; i++) {
                header = &table->headers[i];
                memset(header, 0, sizeof(struct http_decoder_header));
                http_decoder_string_init(&header->key, MAX_HEADER_KEY_CACHE_SIZE);
                http_decoder_string_init(&header->val, MAX_HEADER_VALUE_CACHE_SIZE);
            }
        }
        table->header_index++;
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_commit(&table->body);
        break;
    default:
        abort();
        break;
    }
}

void http_decoder_table_reset(struct http_decoder_table *table, enum http_item type)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    switch (type) {
    case HTTP_ITEM_URI:
        http_decoder_string_reset(&table->uri);
        break;
    case HTTP_ITEM_STATUS:
        http_decoder_string_reset(&table->status);
        break;
    case HTTP_ITEM_METHOD:
        http_decoder_string_reset(&table->method);
        break;
    case HTTP_ITEM_VERSION:
        http_decoder_string_reset(&table->version);
        break;
    case HTTP_ITEM_HDRKEY:
        header = &table->headers[table->header_index];
        http_decoder_string_reset(&header->key);
        break;
    case HTTP_ITEM_HDRVAL:
        header = &table->headers[table->header_index];
        http_decoder_string_reset(&header->val);
        break;
    case HTTP_ITEM_BODY:
        http_decoder_string_reset(&table->body);
        break;
    default:
        abort();
        break;
    }
}

void http_decoder_table_reinit(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    struct http_decoder_header *header = NULL;
    assert(table);

    http_decoder_string_reinit(&table->uri);
    http_decoder_string_reinit(&table->status);
    http_decoder_string_reinit(&table->method);
    http_decoder_string_reinit(&table->version);
    for (size_t i = 0; i < table->header_iter; i++) {
        header = &table->headers[i];
        http_decoder_string_reinit(&header->key);
        http_decoder_string_reinit(&header->val);
    }
        
    http_decoder_string_reinit(&table->body);
}

void http_decoder_table_dump(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    http_decoder_string_dump(&table->uri, "uri");
    http_decoder_string_dump(&table->status, "status");
    http_decoder_string_dump(&table->method, "method");
    http_decoder_string_dump(&table->version, "version");
    http_decoder_string_dump(&table->body, "body");

    for (size_t i = 0; i < table->header_cnt; i++) {
        struct http_decoder_header *header = &table->headers[i];
        if (NULL == header) {
            continue;
        }

        http_decoder_string_dump(&header->key, "key");
        http_decoder_string_dump(&header->val, "val");
    }
}

int http_decoder_table_get_uri(struct http_decoder_table *table, struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->uri, out);
}

int http_decoder_table_get_method(struct http_decoder_table *table, struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->method, out);
}

int http_decoder_table_get_status(struct http_decoder_table *table, struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->status, out);
}

int http_decoder_table_get_version(struct http_decoder_table *table, struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->version, out);
}

int http_decoder_table_get_body(struct http_decoder_table *table, struct hstring *out)
{
    if (NULL == table || NULL == out) {
        return -1;
    }

    return http_decoder_string_get(&table->body, out);
}

int http_decoder_table_get_header(struct http_decoder_table *table, struct hstring *key,
                                  struct http_header *hdr_array, size_t array_size)
{
    if (NULL == table || NULL == key->str || 0 == key->str_len) {
        return 0;
    }

    int header_cnt = 0;
    for (size_t i = 0; i < table->header_cnt && header_cnt < array_size; i++) {
        struct http_decoder_header *tmp_header = &table->headers[i];
        if (tmp_header->key.commit.str_len != key->str_len) {
            continue;
        }

        if (http_decoder_string_state(&tmp_header->key) == STRING_STATE_COMMIT &&
            http_decoder_string_state(&tmp_header->val) == STRING_STATE_COMMIT) {
            struct hstring tmp_key;
            http_decoder_string_get(&tmp_header->key, &tmp_key);

            if (tmp_key.str_len == key->str_len &&
                (0 == strncasecmp(tmp_key.str, key->str, key->str_len))) {
                http_decoder_string_get(&tmp_header->key, &hdr_array[header_cnt].key);
                http_decoder_string_get(&tmp_header->val, &hdr_array[header_cnt].val);
                header_cnt++;
            }
        }
    }

    return header_cnt;
}

int http_decoder_table_iter_header(struct http_decoder_table *table,
                                   struct http_header *hdr)
{
    if (NULL == table || NULL == hdr) {
        return -1;
    }

    if (table->header_iter >= table->header_cnt) {
        return 0;
    }

    struct http_decoder_header *tmp_header = &table->headers[table->header_iter];
    if (tmp_header != NULL) {
        if (http_decoder_string_state(&tmp_header->key) == STRING_STATE_COMMIT &&
            http_decoder_string_state(&tmp_header->val) == STRING_STATE_COMMIT) {

            http_decoder_string_get(&tmp_header->key, &hdr->key);
            http_decoder_string_get(&tmp_header->val, &hdr->val);
            table->header_iter++;

            return 1;
        }
    }

    hdr->key.str = NULL;
    hdr->key.str_len = 0;

    hdr->val.str = NULL;
    hdr->val.str_len = 0;

    return 0;
}

int http_decoder_table_has_parsed_header(struct http_decoder_table *table)
{
    if (NULL == table || (table->header_iter == table->header_index)) {
        return 0;
    }

    struct http_decoder_header *tmp_header = &table->headers[table->header_iter];
    if (http_decoder_string_state(&tmp_header->key) == STRING_STATE_COMMIT &&
        http_decoder_string_state(&tmp_header->val) == STRING_STATE_COMMIT) {
        return 1;
    }

    return 0;
}

int http_decoder_table_header_complete(struct http_decoder_table *table)
{
    if (NULL == table) {
        return -1;
    }

    return table->header_complete;
}

void http_decoder_table_set_header_complete(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    table->header_complete = 1;
}

void http_decoder_table_reset_header_complete(struct http_decoder_table *table)
{
    if (NULL == table) {
        return;
    }

    table->header_complete = 0;
}