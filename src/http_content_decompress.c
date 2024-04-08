/*
**********************************************************************************************
*	File: http_content_decompress.c
*   Description: 
*	Authors: LuWenPeng <luwenpeng@geedgenetworks.com>
*	Date:    2022-10-31
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/


#include <zlib.h>
#include <string.h>
#include <assert.h>
#include <brotli/decode.h>

#include "stellar/utils.h"
#include "http_decoder_utils.h"
#include "http_content_decompress.h"

#define BUFFER_SIZE (16 * 1024)

struct http_content_decompress {
    enum http_content_encoding encoding;

    z_stream *z_stream_ptr;
    BrotliDecoderState *br_state;

    char *buffer;
    size_t buffer_size;
};

enum http_content_encoding
http_content_encoding_str2int(const char *content_encoding)
{
    if (strcasestr(content_encoding, "gzip") != NULL) {
        return HTTP_CONTENT_ENCODING_GZIP;
    }

    if (strcasestr(content_encoding, "deflate") != NULL) {
        return HTTP_CONTENT_ENCODING_DEFLATE;
    }

    if (strcasestr(content_encoding, "br") != NULL) {
        return HTTP_CONTENT_ENCODING_BR;
    }

    return HTTP_CONTENT_ENCODING_NONE;
}

const char *
http_content_encoding_int2str(enum http_content_encoding content_encoding)
{
    if (content_encoding == HTTP_CONTENT_ENCODING_GZIP) {
        return "gzip";
    }

    if (content_encoding == HTTP_CONTENT_ENCODING_DEFLATE) {
        return "deflate";
    }

    if (content_encoding == HTTP_CONTENT_ENCODING_BR) {
        return "br";
    }

    return "unknown";
}

struct http_content_decompress *
http_content_decompress_create(enum http_content_encoding encoding)
{
    struct http_content_decompress *decompress =
        CALLOC(struct http_content_decompress, 1);
    assert(decompress);

    decompress->encoding = encoding;
    decompress->z_stream_ptr = NULL;
    decompress->br_state = NULL;

    decompress->buffer = CALLOC(char, BUFFER_SIZE);
    assert(decompress->buffer);
    decompress->buffer_size = BUFFER_SIZE;

    if (encoding == HTTP_CONTENT_ENCODING_GZIP
        || encoding == HTTP_CONTENT_ENCODING_DEFLATE) {
        decompress->z_stream_ptr = CALLOC(z_stream, 1);
        assert(decompress->z_stream_ptr);

        decompress->z_stream_ptr->zalloc = NULL;
        decompress->z_stream_ptr->zfree = NULL;
        decompress->z_stream_ptr->opaque = NULL;
        decompress->z_stream_ptr->avail_in = 0;
        decompress->z_stream_ptr->next_in = Z_NULL;

        if (encoding == HTTP_CONTENT_ENCODING_GZIP) {
            if (inflateInit2(decompress->z_stream_ptr, MAX_WBITS + 16)
                != Z_OK) {
                goto error;
            }
        }

        if (encoding == HTTP_CONTENT_ENCODING_DEFLATE) {
            if (inflateInit2(decompress->z_stream_ptr, -MAX_WBITS) != Z_OK) {
                goto error;
            }
        }
    }

    if (encoding == HTTP_CONTENT_ENCODING_BR) {
        decompress->br_state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
        if (decompress->br_state == NULL) {
            goto error;
        }
    }

    return decompress;

error:
    http_content_decompress_destroy(decompress);
    return NULL;
}

void http_content_decompress_destroy(struct http_content_decompress *decompress)
{
    if (NULL == decompress) {
        return;
    }

    if (decompress->z_stream_ptr != NULL) {
        inflateEnd(decompress->z_stream_ptr);
        FREE(decompress->z_stream_ptr);
    }

    if (decompress->br_state) {
        BrotliDecoderDestroyInstance(decompress->br_state);
        decompress->br_state = NULL;
    }

    FREE(decompress->buffer);
    FREE(decompress);
}

static int
http_content_decompress_write_zlib(struct http_content_decompress *decompress,
                                   const char *indata, size_t indata_len,
                                   char **outdata, size_t *outdata_len)
{
    z_stream *z_stream_ptr = decompress->z_stream_ptr;
    z_stream_ptr->avail_in = (unsigned int)indata_len;
    z_stream_ptr->next_in = (unsigned char *)indata;
    z_stream_ptr->avail_out = (unsigned int)decompress->buffer_size;
    z_stream_ptr->next_out = (unsigned char *)decompress->buffer;

    *outdata = NULL;
    *outdata_len = 0;

    int ret = 0;
    do {
        ret = inflate(z_stream_ptr, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT
            || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            (void)inflateEnd(z_stream_ptr);
            return -1;
        }

        size_t have = decompress->buffer_size - z_stream_ptr->avail_out;
        if (have > 0) {
            if (0 == z_stream_ptr->avail_out) {
                decompress->buffer_size += have;
                decompress->buffer = REALLOC(char, decompress->buffer,
                                             decompress->buffer_size);
                *outdata = decompress->buffer;
                *outdata_len = *outdata_len + have;
                http_decoder_log(DEBUG, "%s realloc outbuffer %zu bytes", 
                                 http_content_encoding_int2str(decompress->encoding), 
                                 decompress->buffer_size);
                z_stream_ptr->avail_out = have;
                z_stream_ptr->next_out = (unsigned char *)decompress->buffer + 
                                         (decompress->buffer_size - have);
            } else {
                *outdata = decompress->buffer;
                *outdata_len = have;
            }
        }
    } while (z_stream_ptr->avail_in != 0);

    return 0;
}

static int
http_content_decompress_write_br(struct http_content_decompress *decompress,
                                 const char *indata, size_t indata_len,
                                 char **outdata, size_t *outdata_len)
{
    size_t available_in = indata_len;
    const unsigned char *next_in = (const unsigned char *)indata;
    size_t available_out = decompress->buffer_size;
    unsigned char *next_out = (unsigned char *)decompress->buffer;

    *outdata = NULL;
    *outdata_len = 0;

    int ret;
    for (;;) {
        ret = BrotliDecoderDecompressStream(decompress->br_state, &available_in, 
                                            &next_in, &available_out, &next_out, 0);
        size_t have = decompress->buffer_size - available_out;
        if (have > 0) {
            if (0 == available_out) {
                decompress->buffer_size += have;
                decompress->buffer = REALLOC(char, decompress->buffer,
                                             decompress->buffer_size);
                *outdata = decompress->buffer;
                *outdata_len = *outdata_len + have;
                available_out = have;
                next_out = (unsigned char *)decompress->buffer +
                           (decompress->buffer_size - have);
            } else {
                *outdata = decompress->buffer;
                *outdata_len = have;
            }
        }

        if (ret == BROTLI_DECODER_RESULT_SUCCESS ||
            ret == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            return 0;
        }

        if (ret == BROTLI_DECODER_RESULT_ERROR) {
            BrotliDecoderErrorCode errcode =
                BrotliDecoderGetErrorCode(decompress->br_state);
            http_decoder_log(ERROR,
                             "BrotliDecoderDecompressStream() failed: errno = %d, %s",
                             errcode, BrotliDecoderErrorString(errcode));
            return -1;
        }

        assert(ret == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);
    }
}

int http_content_decompress_write(struct http_content_decompress *decompress,
                                  const char *indata, size_t indata_len,
                                  char **outdata, size_t *outdata_len)
{
    assert(decompress);
    assert(indata);
    assert(indata_len > 0);
    assert(outdata);
    assert(outdata_len);

    *outdata = NULL;
    *outdata_len = 0;

    if (decompress->encoding == HTTP_CONTENT_ENCODING_GZIP ||
        decompress->encoding == HTTP_CONTENT_ENCODING_DEFLATE) {
        return http_content_decompress_write_zlib(decompress, indata, indata_len,
                                                  outdata, outdata_len);
    }

    if (decompress->encoding == HTTP_CONTENT_ENCODING_BR) {
        return http_content_decompress_write_br(decompress, indata, indata_len,
                                                outdata, outdata_len);
    }

    assert(0);
    return -1;
}