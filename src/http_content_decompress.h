/*
**********************************************************************************************
*	File: http_content_decompress.h
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/


#ifndef _HTTP_CONTENT_DECOMPRESS_H_
#define _HTTP_CONTENT_DECOMPRESS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

enum http_content_encoding {
    HTTP_CONTENT_ENCODING_NONE = 0,
    HTTP_CONTENT_ENCODING_GZIP = 1 << 1,
    HTTP_CONTENT_ENCODING_DEFLATE = 1 << 2,
    HTTP_CONTENT_ENCODING_BR = 1 << 3,
};

struct http_content_decompress;

enum http_content_encoding
http_content_encoding_str2int(const char *content_encoding);

const char *
http_content_encoding_int2str(enum http_content_encoding content_encoding);

struct http_content_decompress *
http_content_decompress_create(enum http_content_encoding encoding);

void http_content_decompress_destroy(struct http_content_decompress *decompress);

// return   0 : success
// return  -1 : error
int http_content_decompress_write(struct http_content_decompress *decompress,
                                  const char *indata, size_t indata_len,
                                  char **outdata, size_t *outdata_len);

#ifdef __cplusplus
}
#endif

#endif