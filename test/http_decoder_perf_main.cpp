#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "http_decoder_inc.h"

#define TIME_MEASURE 1
#if TIME_MEASURE
#define TIME_START()  struct timespec _start_time, _end_time;  clock_gettime(CLOCK_REALTIME, &_start_time)
#define TIME_DIFF()  \
    long long time_diff_ns;\
    do { \
        clock_gettime(CLOCK_REALTIME, &_end_time); \
        if (likely(_end_time.tv_sec == _start_time.tv_sec))\
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

static struct http_topic_exdata_compose g_topic_exdata_set[] = 
{
    {HTTPD_TOPIC_TCP_STREAM_INDEX,  TOPIC_TCP_STREAM, NULL, NULL, "HTTP_DECODER_EXDATA_BASEON_TCP_STREAM", NULL, 0, 0},
    {HTTPD_TOPIC_HTTP_MSG_INDEX,    HTTP_DECODER_TOPIC, NULL, NULL, NULL, NULL, 1, 1},
    {HTTPD_TOPIC_HTTP_TUNNEL_INDEX, HTTP_DECODER_TUNNEL_TOPIC, NULL, NULL, "HTTP_DECODER_EXDATA_BASEON_HTTP_TUNNEL",  NULL, 2, 2},
};

struct packet{
    const char *payload;
    size_t payload_len;
    u_int8_t dir;
};

struct stellar{
    int plugin_id;
    void *plugin_env;
    int exdata_id;
    void *exdata;
    int consumer_topid_id;
    int publish_topic_id;
    session_exdata_free *free_func;
    session_msg_free_cb_func *publish_msg_free_cb;
};

struct session{
    struct stellar *st;
    enum session_state sess_state;
    struct session_addr addr;
    void *context;
    void *exdata;
    void *message;
    const struct packet *current_payload_st;
};

extern "C" void *http_decoder_init(struct stellar *st);
extern "C" void http_decoder_exit(void *plugin_env);
extern "C" void *httpd_session_ctx_new_cb(struct session *sess, void *plugin_env);
extern "C" void _httpd_ex_data_free_cb(struct session *s, int idx,void *ex_data, void *arg);
extern "C" void http_decoder_tcp_stream_msg_cb(struct session *sess, int topic_id, const void *msg, void *no_use_ctx, void *plugin_env);
extern "C" void http_decoder_perf_entry(struct session *sess, int topic_id, const void *raw_msg, void *per_session_ctx, void *plugin_env);

static struct fieldstat_easy *fs4_instance;
static struct fieldstat_tag FS4_SIMPLE_HISGRAM_TAG;
static int fs4_simple_id;

static struct fieldstat_tag FS4_LONG_LONG_URL_HISGRAM_TAG;
static int fs4_long_long_url_id;

static struct fieldstat_tag FS4_FRAG_HISGRAM_TAG;
static int fs4_frag_id;

void stellar_session_plugin_dettach_current_session(struct session *sess)
{
    return;
}

int stellar_session_mq_destroy_topic(struct stellar *st, int topic_id)
{
    return 0;
}
int stellar_get_worker_thread_num(struct stellar *st)
{
    return 1;
}
int stellar_get_current_thread_id(struct stellar *st)
{
    return 0;
}
int session_get_current_thread_id(struct session *sess)
{
    return 0;
}
int stellar_session_plugin_register(struct stellar *st, 
									session_ctx_new_func session_ctx_new, 
									session_ctx_free_func session_ctx_free, 
									void *plugin_env)
{
    return 0;
}                                    
int stellar_session_exdata_new_index(struct stellar *st, const char *name, session_exdata_free *free_func,void *arg)
{
    st->free_func = free_func;
    return 0;
}
int session_exdata_set(struct session *sess, int idx, void *ex_ptr)
{
    sess->exdata = ex_ptr;
    return 0;
}
void *session_exdata_get(struct session *sess, int idx)
{
    return sess->exdata;
}

enum session_state session_get_current_state(struct session *sess)
{
    return sess->sess_state;
}

int stellar_session_mq_get_topic_id(struct stellar *st, const char *topic_name)
{
    // for(int i = 0; i < HTTPD_TOPIC_INDEX_MAX; i++){
    //     if(strcmp(topic_name, "TCP_STREAM") == 0){
    //         st->consumer_topid_id = g_topic_exdata_set[HTTPD_TOPIC_TCP_STREAM_INDEX].sub_topic_id;
    //         st->publish_topic_id = g_topic_exdata_set[HTTPD_TOPIC_HTTP_MSG_INDEX].sub_topic_id;
    //     }        
    //     if(strcmp(topic_name, g_topic_exdata_set[i].topic_name) == 0){
    //         return st->consumer_topid_id;
    //     }
    // }
    return -1;
}

int stellar_session_mq_create_topic(struct stellar *st, const char *topic_name, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{

    for(int i = 0; i < HTTPD_TOPIC_INDEX_MAX; i++){
        if(strcmp(topic_name, g_topic_exdata_set[i].topic_name) == 0){
            if(strcmp(topic_name, HTTP_DECODER_TOPIC) == 0){
                st->publish_msg_free_cb = msg_free_cb;
            }
            if(strcmp(topic_name, "TCP_STREAM") == 0){
                st->publish_topic_id = g_topic_exdata_set[HTTPD_TOPIC_HTTP_MSG_INDEX].sub_topic_id;
            }
            return g_topic_exdata_set[i].sub_topic_id;
        }
    }
    return -1;
}
const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
{
    const struct packet *test_payload = sess->current_payload_st;
    *payload_len = test_payload->payload_len;
    return test_payload->payload;
}


struct session_addr *session_get0_addr(struct session *sess, enum session_addr_type *addr_type)
{
    *addr_type = SESSION_ADDR_TYPE_IPV4_TCP;
    return &sess->addr;
}

int packet_get_direction(const struct packet *pkt)
{
    return pkt->dir;
}

const struct packet *session_get0_current_packet(struct session *sess)
{
    return sess->current_payload_st;
}
int stellar_session_mq_subscribe(struct stellar *st, int topic_id, on_session_msg_cb_func *plugin_on_msg_cb, int plugin_id)
{
    return 0;
}
int session_mq_publish_message(struct session *sess, int topic_id, void *msg)
{
    if(topic_id == g_topic_exdata_set[HTTPD_TOPIC_HTTP_MSG_INDEX].sub_topic_id){
        http_decoder_perf_entry(sess, topic_id, msg, NULL, NULL);
        //maybe generate many messages for one packet, so free msg immediately
        sess->st->publish_msg_free_cb(sess, msg, NULL);
    }
    return 0;
}

int session_is_symmetric(struct session *sess, unsigned char *flag)
{
    *flag = (SESSION_SEEN_C2S_FLOW | SESSION_SEEN_S2C_FLOW);
    return 1;
}

int session_mq_ignore_message(struct session *sess, int topic_id, int plugin_id)
{
    return 0;
}

static void perf_test_init_per_session(struct session *sess)
{
    sess->context = httpd_session_ctx_new_cb(sess, sess->st->plugin_env);
}

static void perf_test_free_per_session(struct session *sess)
{
    sess->st->free_func(sess, sess->st->exdata_id, sess->exdata, NULL);
}

static void perf_test_loop(struct stellar *st, struct session *sess, struct packet *test_payload, int test_payload_index_max, const struct fieldstat_tag *tag, int fs4_metric_id)
{
    sess->current_payload_st = &test_payload[0];
    perf_test_init_per_session(sess);
    sess->sess_state = SESSION_STATE_OPENING;
    TIME_START();

    for(int i = 0; i < test_payload_index_max; i++)
    {
        sess->current_payload_st = &test_payload[i];
        http_decoder_tcp_stream_msg_cb(sess, g_topic_exdata_set[HTTPD_TOPIC_TCP_STREAM_INDEX].sub_topic_id, test_payload[i].payload, NULL, sess->st->plugin_env);
        TIME_DIFF();
        fieldstat_easy_histogram_record(fs4_instance, 0, fs4_metric_id, tag, 1, time_diff_ns);
        sess->sess_state = SESSION_STATE_ACTIVE;
    }
    sess->sess_state = SESSION_STATE_CLOSING;
    perf_test_free_per_session(sess);
}

static void perf_test_load_plug(struct stellar *st)
{
    st->plugin_env = http_decoder_init(st);
    assert(st->plugin_env != NULL);
    return;
}

static void perf_test_init(struct stellar *st, struct session *sess)
{
    memset(st, 0, sizeof(struct stellar));
    memset(sess, 0, sizeof(struct session));
    st->plugin_id = 0;
    st->exdata_id = 0;
    st->publish_topic_id = -1;  //init
    st->consumer_topid_id = 2; //init
    perf_test_load_plug(st);

    sess->st = st;
    sess->addr.ipv4.saddr = 0x01020304;
    sess->addr.ipv4.daddr = 0x05060708;
    sess->addr.ipv4.sport = 12345;
    sess->addr.ipv4.dport = 80;
}

#define SET_DATA_LENGTH_DIR(pkt_payload, cstr_in_heap,  cur_dir, tmp_index) \
    pkt_payload.payload = cstr_in_heap; \
    pkt_payload.payload_len = strlen(cstr_in_heap); \
    pkt_payload.dir = cur_dir;\
    tmp_index++;

static void init_test_data_simple(struct packet *test_payload, int *index)
{
    int tmp_index = 0;
    const char *c2s_payload = "GET / HTTP/1.1\r\nHost: www.simple.com\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9\r\n\r\n";
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], c2s_payload, PACKET_DIRECTION_C2S, tmp_index);

    const char *content = "Hello, http decoder perf test simple!!!";
    int content_length = strlen(content);
    char *s2c_payload = (char *)malloc(1024);
    snprintf(s2c_payload, 1024, "HTTP/1.1 200 OK\r\nServer: Apache-Coyote/1.1\r\nConnection: keep-alive\r\nDate: Sat, 01 May 2024 01:36:57 GMT\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: %d\r\n\r\n%s", content_length, content);
    
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], s2c_payload, PACKET_DIRECTION_S2C, tmp_index);

    *index = tmp_index;
}

static void init_test_data_frag(struct packet *test_payload, int *index)
{
    int tmp_index = 0;
    /* c2s */
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "GET / HTTP/1.1\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Host: www.fragment.com\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Cache-Control: max-age=0\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Connection: keep-alive\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Referer: http://fragment.com/register.jsp?redirect:http://aa.bb.cc.dd.com/?\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Cookie: JSESSIONID=385C79E211D561C0CA13D90F150F603D34875GH87FSHG8S7RTHG74875GHS8R7THG87SRTH\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Accept: */*\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Accept-Encoding: gzip, deflate\r\n", PACKET_DIRECTION_C2S, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "\r\n", PACKET_DIRECTION_C2S, tmp_index); //header EOF

    /* s2c */
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "HTTP/1.1 200 OK\r\n", PACKET_DIRECTION_S2C, tmp_index); 
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Server: Apache-Coyote/1.1\r\n", PACKET_DIRECTION_S2C, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Connection: keep-alive\r\n", PACKET_DIRECTION_S2C, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Content-Type: text/html;charset=UTF-8\r\n", PACKET_DIRECTION_S2C, tmp_index);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "Date: Sat, 01 May 2024 01:36:57 GMT\r\n", PACKET_DIRECTION_S2C, tmp_index);
    
    char *cont_len_buf = (char *)malloc(1024);
    const char *s2c_payload = "Hello, http decoder perf test fragment!!!";
    int content_length = strlen(s2c_payload);
    snprintf(cont_len_buf, 1024, "Content-Length: %d\r\n\r\n", content_length);
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], cont_len_buf, PACKET_DIRECTION_S2C, tmp_index); 

    SET_DATA_LENGTH_DIR(test_payload[tmp_index], "\r\n", PACKET_DIRECTION_S2C, tmp_index); //header EOF

    SET_DATA_LENGTH_DIR(test_payload[tmp_index], s2c_payload, PACKET_DIRECTION_S2C, tmp_index); 

    *index = tmp_index;
}

static void init_test_data_long_long_url(struct packet *test_payload, int *index)
{
    int tmp_index = 0;

    const char *request_line_frag1 = "GET /long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/";
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], request_line_frag1, PACKET_DIRECTION_C2S, tmp_index);

    const char *request_line_frag2 = "long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/\
/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long/long.index.html HTTP/1.1\r\n";
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], request_line_frag2, PACKET_DIRECTION_C2S, tmp_index);

    const char *request_line_frag3 = "Host: www.long-long-url.com\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nUpgrade-Insecure-Requests: 1\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\nAccept-Encoding: gzip, deflate\r\nAccept-Language: zh-CN,zh;q=0.9\r\n\r\n";
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], request_line_frag3, PACKET_DIRECTION_C2S, tmp_index);

    const char *content = "Hello, http decoder perf test long long url!!!";
    int content_length = strlen(content);
    char *s2c_payload = (char *)malloc(1024);
    snprintf(s2c_payload, 1024, "HTTP/1.1 200 OK\r\nServer: Apache-Coyote/1.1\r\nConnection: keep-alive\r\nDate: Sat, 01 May 2024 01:36:57 GMT\r\nContent-Type: text/html;charset=UTF-8\r\nContent-Length: %d\r\n\r\n%s", content_length, content);
    
    SET_DATA_LENGTH_DIR(test_payload[tmp_index], s2c_payload, PACKET_DIRECTION_S2C, tmp_index);

    *index = tmp_index;
}

static void perf_stat_init(void)
{
    fs4_instance = fieldstat_easy_new(1, "http_decoder_test", NULL, 0);
    fieldstat_easy_enable_auto_output(fs4_instance, "./httpd_hisgram.json", 1);

    FS4_SIMPLE_HISGRAM_TAG.key = "simple";
    FS4_SIMPLE_HISGRAM_TAG.type = TAG_DOUBLE;
    FS4_SIMPLE_HISGRAM_TAG.value_double = 0.00001;
    fs4_simple_id = fieldstat_easy_register_histogram(fs4_instance, "simple", 1, 99999999, 5);

    FS4_FRAG_HISGRAM_TAG.key = "frag";
    FS4_FRAG_HISGRAM_TAG.type = TAG_DOUBLE;
    FS4_FRAG_HISGRAM_TAG.value_double = 0.00001;
    fs4_frag_id = fieldstat_easy_register_histogram(fs4_instance, "frag", 1, 99999999, 5);

    FS4_LONG_LONG_URL_HISGRAM_TAG.key = "long-url";
    FS4_LONG_LONG_URL_HISGRAM_TAG.type = TAG_DOUBLE;
    FS4_LONG_LONG_URL_HISGRAM_TAG.value_double = 0.00001;
    fs4_long_long_url_id = fieldstat_easy_register_histogram(fs4_instance, "long-url", 1, 99999999, 5);
}

int main(int argc, char const *argv[])
{
    struct stellar st;
    struct session sess;
    struct packet test_payload_simple [4] = {};
    int payload_index_simple = 0;

    struct packet test_payload_long_long_url [8] = {};
    int payload_index_long_long_url = 0;

    struct packet test_payload_frag [32] = {};
    int payload_index_frag = 0;

    perf_test_init(&st, &sess);
    perf_stat_init();

    init_test_data_simple(test_payload_simple, &payload_index_simple);
    init_test_data_long_long_url(test_payload_long_long_url, &payload_index_long_long_url);
    init_test_data_frag(test_payload_frag, &payload_index_frag);

    while(1){
        perf_test_loop(&st, &sess, test_payload_simple, payload_index_simple, &FS4_SIMPLE_HISGRAM_TAG, fs4_simple_id);
        perf_test_loop(&st, &sess, test_payload_long_long_url, payload_index_long_long_url, &FS4_LONG_LONG_URL_HISGRAM_TAG, fs4_long_long_url_id);
        perf_test_loop(&st, &sess, test_payload_frag, payload_index_frag, &FS4_FRAG_HISGRAM_TAG, fs4_frag_id);
    }
    return 0;
}
