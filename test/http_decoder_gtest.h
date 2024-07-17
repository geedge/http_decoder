#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
#include <stellar/session.h>
#include <stellar/session_mq.h>
#include <stellar/session_exdata.h>
#include <stellar/stellar.h>
#ifdef __cplusplus
}
#endif
#include "http_decoder.h"
#include "md5.h"
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pcap/pcap.h>
#include "cJSON.h"

#define TRUE 1
#define FLASE 0

#define JSON_KEY_VALUE_STRING_MAX_LEN (4096)

#ifndef MAX
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) >= (b) ? (b) : (a))
#endif

#define MMALLOC(type, size) ((type *)calloc(1, size))
#define MFREE(p)  \
    do            \
    {             \
        free(p);  \
        p = NULL; \
    } while (0)

#if 0
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#define EX_DATA_MAX_SIZE 10
#define PIPELINE_MAX_NUM 8

#define GTEST_FIX_PAYLOAD_CSTR "<Hello http decoder World!!!>"
#define GTEST_FIX_PAYLOAD_MD5 "e91e072f772737c7a45013cc3b1a916c"

#define GTEST_HTTP_URL_NAME "__X_HTTP_URL"
#define GTEST_HTTP_TRANS_NAME "__X_HTTP_TRANSACTION"
#define GTEST_HTTP_TRANS_SEQ_NAME "__X_HTTP_TRANSACTION_SEQ"
#define GTEST_HTTP_TUPLE4_NAME "__X_HTTP_TUPLE4"
#define GTEST_HTTP_PAYLOAD_NAME "__X_HTTP_PAYLOAD"
#define GTEST_HTTP_PAYLOAD_MD5_NAME "__X_HTTP_PAYLOAD_MD5"

struct stellar *stellar_init(void);
void stellar_destroy(struct stellar *st);
int stellar_load_plugin(struct stellar *st, void *(plugin_init_cb)(struct stellar *st));
struct session *stellar_session_new(struct stellar *st, int topic_id, const char *payload, size_t payload_len, u_int8_t dir);
void stellar_session_active(struct stellar *st, struct session *sess, int topic_id, const char *payload, size_t payload_len, u_int8_t dir);
void stellar_session_close(struct stellar *st, struct session *sess, int topic_id);