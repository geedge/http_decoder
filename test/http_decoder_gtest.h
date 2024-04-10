#pragma once

#include "http_decoder.h"
#include "session.h"
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

struct fake_exdata_manage
{
    char *name;
    int name_len;
    void *arg;
    void (*free_func)(struct session *sess, int idx, void *ex_ptr, void *arg);
};

struct fake_packet
{
    int dir;
    size_t raw_pkt_data_len;
    const char *raw_pkt_data; // referred to packet Ethernet MAC header
    const char *l4_header;    // referred to TCP, UDP header
    size_t payload_data_len;
    const char *payload_data; // referred to L7 data, such as HTTP, DNS, etc.
    size_t payload_submit_offset;
};

enum data_source_type_t
{
    __DATA_SOURCE_NULL = 0,
    DATA_SOURCE_PCAP,
    DATA_SOURCE_JSON,
    __DATA_SOURCE_MAX
};

struct data_src_json_para_t
{
    cJSON *json_root;
    cJSON *current_object;
    int json_array_size;
    int current_json_array_idx;
    char key_value_buf[JSON_KEY_VALUE_STRING_MAX_LEN];
};

struct fake_stellar
{
    struct fake_exdata_manage fake_exdata_mgr[EX_DATA_MAX_SIZE];
    int tcp_mss;
    void *http_decoder_ctx;
    void *http_http_plug_ctx;
    int (*http_decoder_entry)(struct session *sess, int events,
                              const struct packet *pkt, void *cb_arg);
    cJSON *http_plug_test_result_root;
    enum data_source_type_t data_source_type;
    const char *benchmark_json_file_name;
    const char *data_source_file_name;
    cJSON *load_benchmark_json_root;
    pcap_t *pcap_ins;

    struct data_src_json_para_t data_src_json_para;
};

struct fake_session
{
    enum session_type type;
    struct fake_stellar *fst;
    struct fake_packet *fpkt;
    int events;
    int tcp_mss;
    enum session_addr_type addr_type;
    struct session_addr *addr;
    char *readable_addr_cstr;
    void *plug_exdata_array[EX_DATA_MAX_SIZE];
};
