/*
**********************************************************************************************
*	File: http_decoder.h
*   Description: 
*	Authors: Liu WenTan <liuwentan@geedgenetworks.com>
*	Date:    2024-01-10
*   Copyright: (c) Since 2022 Geedge Networks, Ltd. All rights reserved.
***********************************************************************************************
*/

#ifndef _HTTP_DECODER_H_
#define _HTTP_DECODER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

enum http_message_type {
    HTTP_MESSAGE_REQ_LINE,
    HTTP_MESSAGE_REQ_HEADER,
    HTTP_MESSAGE_REQ_BODY,
    HTTP_MESSAGE_RES_LINE,
    HTTP_MESSAGE_RES_HEADER,
    HTTP_MESSAGE_RES_BODY,
    HTTP_MESSAGE_MAX
};

//http string
struct hstring {
    char *str;
    size_t str_len;
};

struct http_header {
    struct hstring key;
    struct hstring val;
};

struct http_request_line {
    struct hstring method;
    struct hstring uri;
    struct hstring version;

    int major_version;
    int minor_version;
};

struct http_response_line {
    struct hstring version;
    struct hstring status;

    int major_version;
    int minor_version;
    int status_code;
};

struct http_message;

enum http_message_type http_message_type(struct http_message *msg);

/**
 * @retval succeed(0) failed(-1)
*/
int http_message_get_request_line(struct http_message *msg,
                                  struct http_request_line *line);

int http_message_get_response_line(struct http_message *msg,
                                   struct http_response_line *line);
/*
* @retval succeed(0) failed(-1)
*/
int http_message_get_header(struct http_message *msg, struct hstring *key,
                            struct http_header *hdr_result);

/**
 * @brief loop reading all headers
 * 
 * @retval succeed(0) failed(<= 0)
*/
int http_message_header_next(struct http_message *msg,
                            struct http_header *header);

int http_message_reset_header_iter(struct http_message *msg);

/**
 * @retval succeed(0) failed(-1)
*/
int http_message_get_raw_body(struct http_message *msg,
                              struct hstring *body);


/**
 * @brief If the body hasn't been compressed, return raw body
 * 
 * @retval succeed(0) failed(-1)
*/
int http_message_get_decompress_body(struct http_message *msg,
                                     struct hstring *body);

/**
 * @retval succeed(0) failed(-1)
*/
int http_message_get_url(struct http_message *msg, struct hstring *url);

/**
 * @retval succeed(>=0) failed(-1)
*/
int http_message_get_transaction_seq(struct http_message *msg);

#ifdef __cplusplus
}
#endif

#endif