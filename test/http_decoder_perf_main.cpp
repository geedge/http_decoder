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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fieldstat/fieldstat_easy.h>
#include "http_decoder_gtest.h"

#define TIME_MEASURE 1
#if TIME_MEASURE
#define TIME_START()  struct timespec _start_time, _end_time; long long time_diff_ns; clock_gettime(CLOCK_REALTIME, &_start_time)
#define TIME_UPDATE()  clock_gettime(CLOCK_REALTIME, &_start_time)
#define TIME_DIFF()  \
    do { \
        clock_gettime(CLOCK_REALTIME, &_end_time); \
        if (_end_time.tv_sec == _start_time.tv_sec)\
        {\
            time_diff_ns = (_end_time.tv_nsec - _start_time.tv_nsec);\
        }else{\
            time_diff_ns = (_end_time.tv_sec * 1000 * 1000 * 1000 + _end_time.tv_nsec) - (_start_time.tv_sec * 1000 * 1000 * 1000 + _start_time.tv_nsec);\
        }\
    }while (0)
#else
#define TIME_START()  
#define TIME_DIFF()
#endif

struct test_packet{
    const char *payload;
    size_t payload_len;
    u_int8_t dir;
};
extern "C" void *http_decoder_perf_plug_init(struct stellar *st);
extern "C" void *http_decoder_init(struct stellar *st);
extern "C" void http_decoder_exit(void *plugin_env);
extern "C" void *httpd_session_ctx_new_cb(struct session *sess, void *plugin_env);
extern "C" void _httpd_ex_data_free_cb(struct session *s, int idx,void *ex_data, void *arg);
extern "C" void http_decoder_tcp_stream_msg_cb(struct session *sess, int topic_id, const void *msg, void *no_use_ctx, void *plugin_env);
extern "C" void http_decoder_perf_entry(struct session *sess, int topic_id, const void *raw_msg, void *per_session_ctx, void *plugin_env);

static struct fieldstat_easy *fs4_instance;
static int fs4_simple_id;
static int fs4_long_long_url_id;
static int fs4_frag_id;

static void perf_test_loop(struct stellar *st, int tcp_stream_topic_id, struct test_packet *test_payload, int test_payload_index_max, int fs4_metric_id)
{
    int idx = 0;
    struct session *sess;
    TIME_START();
    sess = stellar_session_new(st, tcp_stream_topic_id, test_payload[idx].payload, test_payload[idx].payload_len, test_payload[idx].dir);
    idx++;
    TIME_DIFF();
    fieldstat_easy_histogram_record(fs4_instance, 0, fs4_metric_id, NULL, 0, time_diff_ns);

    for(; idx < test_payload_index_max; idx++){
        TIME_UPDATE();
        stellar_session_active(st, sess, tcp_stream_topic_id, test_payload[idx].payload, test_payload[idx].payload_len, test_payload[idx].dir);
        TIME_DIFF();
        fieldstat_easy_histogram_record(fs4_instance, 0, fs4_metric_id, NULL, 0, time_diff_ns);
    }

    TIME_UPDATE();
    stellar_session_close(st, sess, tcp_stream_topic_id);
    TIME_DIFF();
    fieldstat_easy_histogram_record(fs4_instance, 0, fs4_metric_id, NULL, 0, time_diff_ns);
}

#define SET_DATA_LENGTH_DIR(pkt_payload, cstr_in_heap,  cur_dir, tmp_index) \
    pkt_payload.payload = cstr_in_heap; \
    pkt_payload.payload_len = strlen(pkt_payload.payload); \
    pkt_payload.dir = cur_dir;\
    tmp_index++;

static void init_test_data_simple(struct test_packet *test_payload, int *index)
{
    int tmp_index = 0;
    const char *c2s_payload = strdup("GET / HTTP/1.1\r\nHost: www.simple.com\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9\r\n\r\n");
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], c2s_payload, PACKET_DIRECTION_C2S, tmp_index);

    const char *content = "Hello, http decoder perf test simple!!!";
    int content_length = strlen(content);
    char *s2c_payload = (char *)malloc(1024);
    snprintf(s2c_payload, 1024, "HTTP/1.1 200 OK\r\nServer: Apache-Coyote/1.1\r\nConnection: keep-alive\r\nDate: Sat, 01 May 2024 01:36:57 GMT\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: %d\r\n\r\n%s", content_length, content);
    
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], s2c_payload, PACKET_DIRECTION_S2C, tmp_index);
    *index = tmp_index;
}

static void init_test_data_frag(struct test_packet *test_payload, int *index)
{
    int tmp_index = 0;
    /* c2s */
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("GET / HTTP/1.1\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Host: www.fragment.com\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Cache-Control: max-age=0\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Connection: keep-alive\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Referer: http://fragment.com/register.jsp?redirect:http://aa.bb.cc.dd.com/?\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Cookie: JSESSIONID=385C79E211D561C0CA13D90F150F603D34875GH87FSHG8S7RTHG74875GHS8R7THG87SRTH\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Accept: */*\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Accept-Encoding: gzip, deflate\r\n"), PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("\r\n"), PACKET_DIRECTION_C2S, tmp_index); //header EOF

    /* s2c */
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("HTTP/1.1 200 OK\r\n"), PACKET_DIRECTION_S2C, tmp_index); 
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Server: Apache-Coyote/1.1\r\n"), PACKET_DIRECTION_S2C, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Connection: keep-alive\r\n"), PACKET_DIRECTION_S2C, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Content-Type: text/html;charset=UTF-8\r\n"), PACKET_DIRECTION_S2C, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("Date: Sat, 01 May 2024 01:36:57 GMT\r\n"), PACKET_DIRECTION_S2C, tmp_index);
    
    char *cont_len_buf = (char *)malloc(1024);
    const char *s2c_payload = strdup("Hello, http decoder perf test fragment!!!");
    int content_length = strlen(s2c_payload);
    snprintf(cont_len_buf, 1024, "Content-Length: %d\r\n", content_length);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], cont_len_buf, PACKET_DIRECTION_S2C, tmp_index); 

    SET_DATA_LENGTH_DIR(test_payload[tmp_index], strdup("\r\n"), PACKET_DIRECTION_S2C, tmp_index); //header EOF
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], s2c_payload, PACKET_DIRECTION_S2C, tmp_index); 
    *index = tmp_index;
}

static void init_test_data_long_long_url(struct test_packet *test_payload, int *index)
{
    int tmp_index = 0;

    const char *request_line_frag1 = strdup("GET /long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/");
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], request_line_frag1, PACKET_DIRECTION_C2S, tmp_index);

    const char *request_line_frag2 = strdup("long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long.index.html HTTP/1.1\r\n");
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], request_line_frag2, PACKET_DIRECTION_C2S, tmp_index);

    const char *request_line_frag3 = strdup("Host: www.long-long-url.com\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9\r\n\r\n");
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], request_line_frag3, PACKET_DIRECTION_C2S, tmp_index);

    const char *content = "Hello, http decoder perf test long long url!!!";
    int content_length = strlen(content);
    char *s2c_payload = (char *)malloc(1024);
    snprintf(s2c_payload, 1024, "HTTP/1.1 200 OK\r\nServer: Apache-Coyote/1.1\r\nConnection: keep-alive\r\nDate: Sat, 01 May 2024 01:36:57 GMT\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: %d\r\n\r\n%s", content_length, content);
    
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], s2c_payload, PACKET_DIRECTION_S2C, tmp_index);
    *index = tmp_index;
}

static void test_payload_free(struct test_packet *test_payload, int test_payload_index_max)
{
    for(int i = 0; i < test_payload_index_max; i++){
        if(test_payload[i].payload){
            free((void *)test_payload[i].payload);
        }
    }
}

static void perf_stat_init(void)
{
    fs4_instance = fieldstat_easy_new(1, "http_decoder_test", NULL, 0);
    fieldstat_easy_enable_auto_output(fs4_instance, "./httpd_hisgram.json", 1);

    fs4_simple_id = fieldstat_easy_register_histogram(fs4_instance, "simple", 1, 99999999, 5);
    fs4_frag_id = fieldstat_easy_register_histogram(fs4_instance, "frag", 1, 99999999, 5);
    fs4_long_long_url_id = fieldstat_easy_register_histogram(fs4_instance, "long-url", 1, 99999999, 5);
}

int main(int argc, char const *argv[])
{
    struct test_packet test_payload_simple [4] = {};
    int payload_index_simple = 0;
    struct test_packet test_payload_long_long_url [8] = {};
    int payload_index_long_long_url = 0;
    struct test_packet test_payload_frag [32] = {};
    int payload_index_frag = 0;

    struct stellar *st = stellar_init();

    int tcp_stream_topic_id = stellar_session_mq_get_topic_id(st, TOPIC_TCP_STREAM);

    if(stellar_load_plugin(st, http_decoder_init) < 0){
        fprintf(stderr, "load plugin 'http_decoder_init' failed\n");
        return -1;
    }

    if(stellar_load_plugin(st, http_decoder_perf_plug_init) < 0){
        fprintf(stderr, "load plugin 'http_decoder_perf_plug_init' failed\n");
        return -1;
    }

    perf_stat_init();
    init_test_data_simple(test_payload_simple, &payload_index_simple);
    init_test_data_long_long_url(test_payload_long_long_url, &payload_index_long_long_url);
    init_test_data_frag(test_payload_frag, &payload_index_frag);

    // while(1){
    for(int i = 0; i < 1000; i++){
        perf_test_loop(st, tcp_stream_topic_id,test_payload_simple, payload_index_simple, fs4_simple_id);
        perf_test_loop(st, tcp_stream_topic_id,test_payload_long_long_url, payload_index_long_long_url, fs4_long_long_url_id);
        perf_test_loop(st, tcp_stream_topic_id,test_payload_frag, payload_index_frag, fs4_frag_id);
    }

    test_payload_free(test_payload_simple, sizeof(test_payload_simple)/sizeof(struct test_packet)) ;
    test_payload_free(test_payload_long_long_url, sizeof(test_payload_long_long_url)/sizeof(struct test_packet)) ;
    test_payload_free(test_payload_frag, sizeof(test_payload_frag)/sizeof(struct test_packet)) ;
    stellar_destroy(st);
    sleep(1);
    fieldstat_easy_free(fs4_instance);
    return 0;
}
