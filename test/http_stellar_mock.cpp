#include "uthash-2.3.0/include/utlist.h"
#include "http_decoder_gtest.h"
#include "http_stellar_mock.h"

static int G_TCP_STREAM_TOPIC_ID = -1;
static int G_UDP_TOPIC_ID = -1;
static struct stellar *G_STELLAR;
static struct plugin_mgr *g_plugin_mgr_list_head = NULL;
static struct stellar *g_st;

static void stellar_internal_msg_free_cb_func(struct session *sess, void *msg, void *msg_free_arg) {/* do nothing*/}
static void stellar_register_internal_topic(struct stellar *st)
{
    G_TCP_STREAM_TOPIC_ID = stellar_session_mq_get_topic_id(st, TOPIC_TCP_STREAM);
    if(G_TCP_STREAM_TOPIC_ID < 0){
        G_TCP_STREAM_TOPIC_ID = stellar_session_mq_create_topic(st, TOPIC_TCP_STREAM, stellar_internal_msg_free_cb_func, NULL);
    }
    G_UDP_TOPIC_ID = stellar_session_mq_get_topic_id(st, TOPIC_UDP);
    if(G_UDP_TOPIC_ID < 0){
        G_UDP_TOPIC_ID = stellar_session_mq_create_topic(st, TOPIC_UDP, stellar_internal_msg_free_cb_func, NULL);
    }
}
void stellar_session_plugin_dettach_current_session(struct session *sess) { return; }
int stellar_session_mq_destroy_topic(struct stellar *st, int topic_id) { return 0; }
int stellar_get_worker_thread_num(struct stellar *st) { return 1; }
int stellar_get_current_thread_id(struct stellar *st) { return 0; }
int session_get_current_thread_id(struct session *sess) { return 0; }  
int session_mq_ignore_message(struct session *sess, int topic_id, int plugin_id) { return 0; }
int session_mq_unignore_message(struct session *sess, int topic_id, int plugin_id) { return 0; }
int stellar_session_exdata_new_index(struct stellar *st, const char *name, session_exdata_free *free_func,void *arg)
{
    int list_count = 0;
    struct exdata_mgr *tmp, *new_exdata = (struct exdata_mgr *)calloc(1, sizeof(struct exdata_mgr));
    DL_FOREACH(st->exdata_mgr_head, tmp){
        if(0 == strcmp(tmp->name, name) && (strlen(tmp->name) == strlen(name))){
            return 0; //already exist
        }
    }
    DL_COUNT(st->exdata_mgr_head, tmp, list_count);
    new_exdata->exdata_id = list_count;
    new_exdata->name = strdup(name);
    new_exdata->free_func = free_func;
    new_exdata->arg = arg;
    DL_APPEND(st->exdata_mgr_head, new_exdata);
    return 0;
}
int session_exdata_set(struct session *sess, int idx, void *ex_ptr)
{
    struct exdata_mgr *el = NULL;
    DL_FOREACH(sess->runtime_exdata_head, el){
        if(el->exdata_id == idx){
            el->user_ptr = ex_ptr;
            return 0;
        }
    }
    struct exdata_mgr *new_exdata = (struct exdata_mgr *)calloc(1, sizeof(struct exdata_mgr));
    new_exdata->exdata_id = idx;
    new_exdata->user_ptr = ex_ptr;
    DL_APPEND(sess->runtime_exdata_head, new_exdata);
    return 0;
}
void *session_exdata_get(struct session *sess, int idx)
{
    struct exdata_mgr *el = NULL;
    DL_FOREACH(sess->runtime_exdata_head, el){
        if(el->exdata_id == idx){
            return el->user_ptr;
        }
    }
    return NULL;
}
enum session_state session_get_current_state(struct session *sess)
{
    return sess->sess_state;
}
const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
{
    const struct packet *test_payload = &sess->pkt;
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
    return &sess->pkt;
}
int session_is_symmetric(struct session *sess, unsigned char *flag)
{
    *flag = (SESSION_SEEN_C2S_FLOW | SESSION_SEEN_S2C_FLOW);
    return 1;
}
static struct plugin_mgr *get_stat_by_plugin_id(int plugin_id)
{
    struct plugin_mgr *plugin = NULL;
    DL_FOREACH(g_plugin_mgr_list_head, plugin){
        if(plugin->plugin_id == plugin_id){
            break;
        }
    }
    return plugin;
}
int stellar_session_mq_subscribe(struct stellar *st, int topic_id, on_session_msg_cb_func *plugin_on_msg_cb, int plugin_id)
{
    struct topic_mgr *topic_el = NULL;
    DL_FOREACH(st->topic_mgr_head, topic_el){
        if(topic_el->topic_id == topic_id){
            break;
        }
    }
    if(NULL == topic_el){
        return -1;
    }
    struct plugin_mgr *plugin = get_stat_by_plugin_id(plugin_id);
    struct sub_topic_cb_list *sub_cb_list = (struct sub_topic_cb_list *)calloc(1, sizeof(struct sub_topic_cb_list));
    sub_cb_list->sub_cb = plugin_on_msg_cb;
    sub_cb_list->plugin_id = plugin_id;
    sub_cb_list->plugin_env = plugin->plugin_env;
    DL_APPEND(topic_el->sub_free_cb_list_head, sub_cb_list);
    return 0;
}
int stellar_session_mq_get_topic_id(struct stellar *st, const char *topic_name)
{
    struct topic_mgr *el = NULL;
    DL_FOREACH(st->topic_mgr_head, el){
        if(0 == strcmp(el->topic_name, topic_name) && (strlen(el->topic_name) == strlen(topic_name))){
            return el->topic_id;
        }
    }
    return -1;
}
int stellar_session_mq_create_topic(struct stellar *st, const char *topic_name, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{
    int topic_id = stellar_session_mq_get_topic_id(st, topic_name);
    if(topic_id >= 0){
        return topic_id;//already exist
    }
    struct topic_mgr *tmp, *new_topic = (struct topic_mgr *)calloc(1, sizeof(struct topic_mgr));
    int list_count;
    DL_APPEND(st->topic_mgr_head, new_topic);
    DL_COUNT(st->topic_mgr_head, tmp, list_count);
    new_topic->topic_id = list_count;
    new_topic->pub_free_cb = msg_free_cb;
    new_topic->pub_free_arg = msg_free_arg;
    new_topic->topic_name = strdup(topic_name);
    return new_topic->topic_id;
}

int stellar_session_plugin_register(struct stellar *st, session_ctx_new_func session_ctx_new, 
									session_ctx_free_func session_ctx_free, void *plugin_env)
{
    int list_count;
    struct plugin_mgr *tmp, *plugin = (struct plugin_mgr *)calloc(1, sizeof(struct plugin_mgr));
    DL_APPEND(g_plugin_mgr_list_head, plugin); 
    DL_COUNT(g_plugin_mgr_list_head, tmp, list_count);
    plugin->plugin_id = list_count;
    plugin->plugin_env = plugin_env; 
    plugin->session_ctx_new = session_ctx_new;
    plugin->session_ctx_free = session_ctx_free;
    return plugin->plugin_id;
} 
int session_mq_publish_message(struct session *sess, int topic_id, void *msg)
{
    struct topic_mgr *el = NULL;
    DL_FOREACH(sess->st->topic_mgr_head, el){
        if(el->topic_id == topic_id){
            break;
        }
    }
    if(NULL == el){
        return -1;
    }
    sub_topic_cb_list *sub_cb_node = NULL;
    DL_FOREACH(el->sub_free_cb_list_head, sub_cb_node){
        (*sub_cb_node->sub_cb)(sess, topic_id, msg, NULL, sub_cb_node->plugin_env); //todo
    }
    el->pub_free_cb(sess, msg, el->pub_free_arg);
    return 0;
}
int stellar_load_plugin(struct stellar *st, void *(plugin_init_cb)(struct stellar *st))
{
    plugin_init_cb(st);
    return 0;
}
struct session *stellar_session_new(struct stellar *st, int topic_id, const char *payload, size_t payload_len, u_int8_t dir)
{
    struct session *sess = (struct session *)calloc(1, sizeof(struct session));
    sess->st = st;
    sess->sess_state = SESSION_STATE_OPENING;
    sess->addr.ipv4.saddr = 0x7f000001;
    sess->addr.ipv4.daddr = 0x7f000002;
    sess->addr.ipv4.sport = htons(12345);
    sess->addr.ipv4.dport = htons(80);
    if(G_TCP_STREAM_TOPIC_ID == topic_id){
        sess->addr_type = SESSION_ADDR_TYPE_IPV4_TCP;
    }else{
        sess->addr_type = SESSION_ADDR_TYPE_IPV4_UDP;
    }
    sess->pkt.dir = dir;
    sess->pkt.payload = payload;
    sess->pkt.payload_len = payload_len;
    stellar_message *message = (stellar_message *)&sess->pkt;
    session_mq_publish_message(sess, topic_id, message);
    return sess;
}
void stellar_session_active(struct stellar *st, struct session *sess, int topic_id, const char *payload, size_t payload_len, u_int8_t dir)
{
    sess->sess_state = SESSION_STATE_ACTIVE;
    sess->pkt.dir = dir;
    sess->pkt.payload = payload;
    sess->pkt.payload_len = payload_len;
    stellar_message *message = (stellar_message *)&sess->pkt;
    session_mq_publish_message(sess, topic_id, message);
    return;
}
static void session_plugin_exdata_free(struct stellar *st, struct session *sess, int exdata_idx, void *user_ptr)
{
    struct exdata_mgr *el = NULL;
    DL_FOREACH(st->exdata_mgr_head, el){
        if(el->exdata_id == exdata_idx){
            el->free_func(sess, exdata_idx, user_ptr, el->arg);
        }
    }
}
static void session_plugin_exdata_free_all(struct stellar *st, struct session *sess)
{
    struct exdata_mgr *el = NULL, *tmp = NULL;
    DL_FOREACH_SAFE(sess->runtime_exdata_head, el, tmp){
        session_plugin_exdata_free(st, sess, el->exdata_id, el->user_ptr);
        DL_DELETE(sess->runtime_exdata_head, el);
        free(el);
    }
}
static void stellar_session_free(struct stellar *st, struct session *sess)
{
    session_plugin_exdata_free_all(st, sess);
    free(sess);
}
void stellar_session_close(struct stellar *st, struct session *sess, int topic_id)
{
    sess->sess_state = SESSION_STATE_CLOSING;
    sess->pkt.payload = NULL;
    sess->pkt.payload_len = 0;
    stellar_message *message = (stellar_message *)&sess->pkt;
    session_mq_publish_message(sess, topic_id, message);
    stellar_session_free(st, sess);
    return;
}
struct stellar *stellar_init(void)
{
    if(NULL == g_st){
        g_st = (struct stellar *)calloc(1, sizeof(struct stellar));
    }
    stellar_register_internal_topic(g_st);
    return g_st;
}
static void stellar_sub_topi_mgr_destroy(struct topic_mgr *topic_mgr)
{
    struct sub_topic_cb_list *sub_cb_node = NULL, *tmp = NULL;
    DL_FOREACH_SAFE(topic_mgr->sub_free_cb_list_head, sub_cb_node, tmp){
        DL_DELETE(topic_mgr->sub_free_cb_list_head, sub_cb_node);
        free(sub_cb_node);
    }
    return;
}
void stellar_destroy(struct stellar *st)
{
    struct exdata_mgr *el = NULL, *tmp;
    DL_FOREACH_SAFE(st->exdata_mgr_head, el, tmp){
        free((void *)el->name);
        DL_DELETE(st->exdata_mgr_head, el);
        free(el);
    }
    struct topic_mgr *el_topic, *tmp_topic;
    DL_FOREACH_SAFE(st->topic_mgr_head, el_topic, tmp_topic){
        DL_DELETE(st->topic_mgr_head, el_topic);
        stellar_sub_topi_mgr_destroy(el_topic);
        free((void *)el_topic->topic_name);
        free(el_topic);
    }
    free(st);
    return;
}