#pragma once 

#ifdef __cplusplus
extern "C"
{
#endif
#include <bits/types/struct_iovec.h>

struct http_message;
#define HTTP_DECODER_TOPIC  "HTTP_DECODER_MESSAGE"

enum http_message_type {
    HTTP_TRANSACTION_START,  

    HTTP_MESSAGE_REQ_LINE,  
    HTTP_MESSAGE_REQ_HEADER,
    HTTP_MESSAGE_REQ_HEADER_END,
    HTTP_MESSAGE_REQ_BODY_START,  
    HTTP_MESSAGE_REQ_BODY,
    HTTP_MESSAGE_REQ_BODY_END,
    
    HTTP_MESSAGE_RES_LINE,
    HTTP_MESSAGE_RES_HEADER,
    HTTP_MESSAGE_RES_HEADER_END,
    HTTP_MESSAGE_RES_BODY_START, 
    HTTP_MESSAGE_RES_BODY,
    HTTP_MESSAGE_RES_BODY_END,

    HTTP_TRANSACTION_END, 

    HTTP_MESSAGE_MAX
};

struct http_header {
    struct iovec key;
    struct iovec val;
};

struct http_request_line {
    struct iovec method;
    struct iovec uri;
    struct iovec version;
    int major_version;
    int minor_version;
};

struct http_response_line {
    struct iovec version;
    struct iovec status;
    int major_version;
    int minor_version;
    int status_code;
};

typedef struct iovec hstring;

enum http_message_type http_message_type_get(const struct http_message *msg);

void http_message_request_line_get0(const struct http_message *msg, struct http_request_line *line);

void http_message_response_line_get0(const struct http_message *msg, struct http_response_line *line);

/*
*  Pay attention: key->iov_base is case-insensitive.
*/
void http_message_header_get0(const struct http_message *msg,  const struct iovec *key, struct http_header *hdr_result);

/**
 * @brief loop reading all headers.
 * 
 * @retval succeed( >= 0) failed(-1)
*/
int http_message_header_next(const struct http_message *msg, struct http_header *header);

/**
 * @retval succeed( >= 0) failed(-1)
*/
int http_message_reset_header_iter(struct http_message *msg);

void http_message_raw_body_get0(const struct http_message *msg, struct iovec *body);

/**
 * @brief If the body hasn't been compressed, same as http_message_raw_body_get0().
 * 
*/
void http_message_decompress_body_get0(const struct http_message *msg, struct iovec *body);

//raw 
void http_message_raw_url_get0(const struct http_message *msg, struct iovec *url);

void http_message_decoded_url_get0(const struct http_message *msg, struct iovec *url);

/**
 * @retval succeed( >= 0) failed(-1)
*/
int http_message_get_transaction_seq(const struct http_message *msg);

#ifdef __cplusplus
}
#endif
