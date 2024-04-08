/*
**********************************************************************************************
*	File: http_decoder_string.h
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#ifndef _HTTP_DECODER_STRING_H_
#define _HTTP_DECODER_STRING_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "http_decoder.h"


enum string_state {
    STRING_STATE_INIT,
    STRING_STATE_REFER,
    STRING_STATE_CACHE,
    STRING_STATE_COMMIT,
};

/* state transition diagram
 *       +----------+
 *       |          |
 *      \|/         |
 *    +------+      |
 *    | init |      |
 *    +------+      |
 *       |          |
 * +---->|          |
 * |    \|/         |
 * |  +-------+     |
 * |  | refer |--+  |
 * |  +-------+  |  |
 * |     |       |  |
 * |    \|/      |  |
 * |  +-------+  |  |
 * +--| cache |  |  |
 *    +-------+  |  |
 *       |       |  |
 *       |<------+  |
 *      \|/         |
 *    +--------+    |
 *    | commit |    |
 *    +--------+    |
 *       |          |
 *      \|/         |
 *    +--------+    |
 *    | reset  |----+
 *    +--------+
 */


//http decoder string
struct http_decoder_string {
    struct hstring refer; // shallow copy
    struct hstring cache; // deep copy
    struct hstring commit;

    enum string_state state;
    size_t max_cache_size;
};

void http_decoder_string_refer(struct http_decoder_string *rstr,
                               const char *at, size_t length);

void http_decoder_string_cache(struct http_decoder_string *rstr);

void http_decoder_string_commit(struct http_decoder_string *rstr);

void http_decoder_string_reset(struct http_decoder_string *rstr);

void http_decoder_string_init(struct http_decoder_string *rstr,
                              size_t max_cache_size);

void http_decoder_string_reinit(struct http_decoder_string *rstr);

enum string_state http_decoder_string_state(struct http_decoder_string *rstr);

int http_decoder_string_get(struct http_decoder_string *rstr, struct hstring *out);

void http_decoder_string_dump(struct http_decoder_string *rstr, const char *desc);

#ifdef __cplusplus
}
#endif

#endif