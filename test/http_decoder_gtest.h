#pragma once

#include "http_decoder.h"
#include "stellar/session.h"
#include "stellar/session_mq.h"
#include "md5.h"
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pcap/pcap.h>
#include "cJSON.h"

#define TRUE 1
#define FLASE 0

#define JSON_KEY_VALUE_STRING_MAX_LEN (4096)

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) >= (b) ? (b) : (a))

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