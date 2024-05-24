#ifndef _HTTP_DECODER_UTILS_H_
#define _HTTP_DECODER_UTILS_H_

#include <stdlib.h>
#include <stdio.h>

char *safe_dup(const char *str, size_t len);
const char *http_message_type_to_string(enum http_message_type type);
int http_message_type_is_req(struct session *sess, enum http_message_type msg_type);
int http_event_is_req(enum http_event event);
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
#endif