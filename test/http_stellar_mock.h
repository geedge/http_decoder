#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include </usr/include/bits/types/struct_iovec.h>
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

struct packet{
    const char *payload;
    size_t payload_len;
    u_int8_t dir;  //PACKET_DIRECTION_C2S  PACKET_DIRECTION_S2C
};
typedef struct packet stellar_message;
struct exdata_mgr{
    const char *name;
    int exdata_id;
    void *arg;
    void *user_ptr;
    session_exdata_free *free_func;
    struct exdata_mgr *next, *prev;
};    
struct sub_topic_mgr{
    int topic_id;
    int plugin_id;
    void *plugin_env;
};
struct topic_mgr{
    struct topic_mgr *next, *prev;
    const char *topic_name;
    int topic_id;
    session_msg_free_cb_func *pub_free_cb;
    void *pub_free_arg;
    struct sub_topic_cb_list *sub_free_cb_list_head;
};
struct stellar{
    struct topic_mgr *topic_mgr_head;
    struct exdata_mgr *exdata_mgr_head;
};
struct session{
    struct stellar *st;
    enum session_state sess_state;
    enum session_addr_type addr_type;
    struct session_addr addr;
    void *exdata;
    struct packet pkt;
    struct exdata_mgr *runtime_exdata_head;
};
struct plugin_mgr{
    struct plugin_mgr *next, *prev;
    int plugin_id;
    void *plugin_env;
    session_ctx_new_func *session_ctx_new;
    session_ctx_free_func *session_ctx_free;
};
struct sub_topic_cb_list{
    int plugin_id;
    void *plugin_env;
    on_session_msg_cb_func *sub_cb;
    struct sub_topic_cb_list *next, *prev;
};
