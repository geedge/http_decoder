#ifndef _HTTP_DECODER_STAT_H_
#define _HTTP_DECODER_STAT_H_ 1

#include <fieldstat/fieldstat_easy.h>
enum http_decoder_stat_type
{
    HTTPD_STAT_BYTES_C2S = 0,
    HTTPD_STAT_BYTES_S2C,
    HTTPD_STAT_TCP_SEG_C2S,
    HTTPD_STAT_TCP_SEG_S2C,
    HTTPD_STAT_HEADERS_C2S,
    HTTPD_STAT_HEADERS_S2C,
    HTTPD_STAT_URL_BYTES,
    HTTPD_STAT_SESSION_NEW,
    HTTPD_STAT_SESSION_FREE,
    HTTPD_STAT_SESSION_EXCEPTION, // rst, kickout, lost packet, etc.
    HTTPD_STAT_TUNNEL,
    HTTPD_STAT_TRANSACTION_NEW,
    HTTPD_STAT_TRANSACTION_FREE,
    HTTPD_STAT_ASYMMETRY_SESSION_C2S,
    HTTPD_STAT_ASYMMETRY_SESSION_S2C,
    HTTPD_STAT_ASYMMETRY_TRANSACTION_C2S,
    HTTPD_STAT_ASYMMETRY_TRANSACTION_S2C,
    HTTPD_STAT_PARSE_ERR,
    HTTPD_STAT_MAX,
};

struct hd_stat_config_tuple
{
    enum http_decoder_stat_type type;
    const char *name;
};

struct hd_statistics
{
    long long time_ms[HTTPD_STAT_MAX];
    long long counter[HTTPD_STAT_MAX];
    int batch[HTTPD_STAT_MAX]; //call fieldstat_easy_counter_incrby() per batch
}__attribute__ ((aligned (64)));

struct http_decoder_stat
{
    pthread_t timer_pid;
    long long current_time_ms;
    struct fieldstat_easy *fse;
    int stat_interval_pkts; // call fieldstat_incrby every stat_interval_pkts
    int stat_interval_time; //second
    int field_stat_id[HTTPD_STAT_MAX];
    struct hd_statistics *stats; //size is thread number    
};

int http_decoder_stat_init(struct http_decoder_stat *hd_stat, int thread_max, int stat_interval_pkts, int stat_interval_time);
void http_decoder_stat_free(struct http_decoder_stat *hd_stat);
void http_decoder_stat_update(struct http_decoder_stat *hd_stat, int thread_id, enum http_decoder_stat_type type, long long value);
#endif