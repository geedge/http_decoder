#pragma once 

#include <stdlib.h>
#include <stdio.h>
#include <cstddef>
#ifdef __cplusplus
extern "C"
{
#endif
#include <bits/types/struct_iovec.h>
#include "stellar/stellar.h"
#include "stellar/utils.h"
#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "stellar/session_exdata.h"
#ifdef __cplusplus
}
#endif

char *safe_dup(const char *str, size_t len);
int strncasecmp_safe(const char *fix_s1, const char *dyn_s2, size_t fix_n1, size_t dyn_n2);
const char *http_message_type_to_string(enum http_message_type type);
int http_message_type_is_req(struct session *sess, enum http_message_type msg_type);
int http_event_is_req(enum http_event event);
int stellar_session_mq_get_topic_id_reliable(struct stellar *st, const char *topic_name, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg);
void httpd_url_decode(const char *string, size_t length, char *ostring, size_t *olen);
int httpd_url_is_encoded(const char *url, size_t len);
/******************************************************************************
 * Logger
 ******************************************************************************/

enum http_decoder_log_level {
    DEBUG = 0x11,
    WARN = 0x12,
    INFO = 0x13,
    ERROR = 0x14,
};

#ifndef http_decoder_log
#define http_decoder_log(level, format, ...)                                     \
    {                                                                            \
        switch (level)                                                           \
        {                                                                        \
        case DEBUG:                                                              \
            fprintf(stdout, "HTTP_DECODER [DEBUG] " format "\n", ##__VA_ARGS__); \
            fflush(stdout);                                                      \
            break;                                                               \
        case WARN:                                                               \
            fprintf(stdout, "HTTP_DECODER [WARN] " format "\n", ##__VA_ARGS__);  \
            fflush(stdout);                                                      \
            break;                                                               \
        case INFO:                                                               \
            fprintf(stdout, "HTTP_DECODER [INFO] " format "\n", ##__VA_ARGS__);  \
            fflush(stdout);                                                      \
            break;                                                               \
        case ERROR:                                                              \
            fprintf(stderr, "HTTP_DECODER [ERROR] " format "\n", ##__VA_ARGS__); \
            fflush(stderr);                                                      \
            break;                                                               \
        }                                                                        \
    }
#endif
