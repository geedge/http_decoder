#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "http_decoder_inc.h"

static const struct hd_stat_config_tuple g_httpd_stat_tuple[HTTPD_STAT_MAX] = 
{
    {HTTPD_STAT_BYTES_C2S, "bytes_c2s"},
    {HTTPD_STAT_BYTES_S2C, "bytes_s2c"},
    {HTTPD_STAT_TCP_SEG_C2S, "tcp_seg_c2s"},
    {HTTPD_STAT_TCP_SEG_S2C, "tcp_seg_s2c"},
    {HTTPD_STAT_HEADERS_C2S, "headers_c2s"},
    {HTTPD_STAT_HEADERS_S2C, "headers_s2c"},
    {HTTPD_STAT_URL_BYTES, "url_bytes"},
    {HTTPD_STAT_SESSION_NEW, "session_new"},
    {HTTPD_STAT_SESSION_FREE, "session_free"},
    {HTTPD_STAT_SESSION_EXCEPTION, "sess_exception"},
    {HTTPD_STAT_TUNNEL, "tunnel"},
    {HTTPD_STAT_TRANSACTION_NEW, "trans_new"},
    {HTTPD_STAT_TRANSACTION_FREE, "trans_free"},
    {HTTPD_STAT_ASYMMETRY_SESSION_C2S, "asymmetry_sess_c2s"},
    {HTTPD_STAT_ASYMMETRY_SESSION_S2C, "asymmetry_sess_s2c"},
    {HTTPD_STAT_ASYMMETRY_TRANSACTION_C2S, "asymmetry_trans_c2s"},
    {HTTPD_STAT_ASYMMETRY_TRANSACTION_S2C, "asymmetry_trans_s2c"},
    {HTTPD_STAT_PARSE_ERR, "parse_err"},
}; 

void http_decoder_stat_free(struct http_decoder_stat *hd_stat)
{
    pthread_cancel(hd_stat->timer_pid);
    void *join_res = NULL;
    do{
        pthread_join(hd_stat->timer_pid, &join_res);
    }while(join_res != PTHREAD_CANCELED);

    if(hd_stat->stats != NULL){
        free(hd_stat->stats);
    }
    if(hd_stat->fse != NULL){
        fieldstat_easy_free(hd_stat->fse);
    }
}

static void *httpd_stat_timer_thread(void *arg)
{
    pthread_setname_np(pthread_self(), "http_decoder_timer_thread");
    struct http_decoder_stat *hd_stat = (struct http_decoder_stat *)arg;
    struct timespec res;
    while(1){
        clock_gettime(CLOCK_MONOTONIC, &res);
        hd_stat->current_time_ms = (res.tv_sec * 1000) + (res.tv_nsec / 1000000);
        usleep(800);
    }
    return NULL;
}

int http_decoder_stat_init(struct http_decoder_stat *hd_stat, int thread_max, int stat_interval_pkts, int stat_interval_time)
{
    assert(sizeof(g_httpd_stat_tuple)/sizeof(struct hd_stat_config_tuple) == HTTPD_STAT_MAX);
    hd_stat->fse = fieldstat_easy_new(thread_max, "http_decoder_statistics", NULL, 0);
    if (NULL == hd_stat->fse)
    {
        fprintf(stderr, "fieldstat_easy_new failed.");
        return -1;
    }

    for(int i = 0; i < HTTPD_STAT_MAX; i++)
    {
        hd_stat->field_stat_id[i] = fieldstat_easy_register_counter(hd_stat->fse, g_httpd_stat_tuple[i].name);
        if (hd_stat->field_stat_id[i] < 0)
        {
            fprintf(stderr, "fieldstat_easy_register_counter %s failed.", g_httpd_stat_tuple[i].name);
            return -1;
        }
    }

    hd_stat->stats = (struct hd_statistics *)calloc(thread_max, sizeof(struct hd_statistics));
    hd_stat->stat_interval_pkts = stat_interval_pkts;
   
    int ret = fieldstat_easy_enable_auto_output(hd_stat->fse, FILEDSTAT_OUTPUT_FILE, stat_interval_time);
    if (ret < 0)
    {
        fprintf(stderr, "fieldstat_easy_enable_auto_output failed.");
        return -1;
    }

    pthread_create(&hd_stat->timer_pid, NULL, httpd_stat_timer_thread, hd_stat);

    return 0;
}

void http_decoder_stat_update(struct http_decoder_stat *hd_stat, int thread_id, enum http_decoder_stat_type type, long long value)
{
    assert(hd_stat);
    assert(thread_id >= 0);
    assert(type < HTTPD_STAT_MAX);

    struct hd_statistics *cur_hds = &hd_stat->stats[thread_id];

    cur_hds->counter[type] += value;
    cur_hds->batch[type]++;

    if(cur_hds->batch[type] >= hd_stat->stat_interval_pkts 
      || cur_hds->time_ms[type] + 1000 < hd_stat->current_time_ms){
         fieldstat_easy_counter_incrby(hd_stat->fse, thread_id, hd_stat->field_stat_id[type], NULL, 0, cur_hds->counter[type]);
         cur_hds->counter[type] = 0;
         cur_hds->batch[type] = 0;
         cur_hds->time_ms[type] = hd_stat->current_time_ms;
    }
}