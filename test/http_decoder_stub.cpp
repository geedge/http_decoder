/*
    Http Decoder Google Test stub module
*/
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "http_decoder.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include "session.h"
#include "session_exdata.h"
#include "session_mq.h"
#include "stellar.h"
#include "http_decoder_gtest.h"
#include "MESA_jump_layer.h"

    static int g_topic_id = -1;
    static msg_free_cb_func *g_msg_free_cb = NULL;
    static void *g_msg_free_cb_arg = NULL;

    extern struct fake_stellar *g_fake_stellar;

    int commit_test_result_json(cJSON *node, const char *name)
    {
        assert(node != NULL || name != NULL);
        if (g_fake_stellar->http_plug_test_result_root)
        {
            // cJSON_AddStringToObject(node, "__X_HTTP_RESULT_INDEX", name);
            cJSON_AddItemToArray(g_fake_stellar->http_plug_test_result_root, node);
            return 0;
        }
        return -1;
    }

    int packet_get_direction(const struct packet *pkt)
    {
        struct fake_packet *fpkt = (struct fake_packet *)pkt;
        return fpkt->dir;
    }

    int session_event_assign(struct session_event *ev, struct stellar *st, struct session *sess, int events, session_event_cb_func *cb, void *cb_arg)
    {
        DEBUG_PRINT("todo: fake session_event_assign()\n");
        return 0;
    }

    const char *session_get0_readable_addr(struct session *sess)
    {
        struct fake_session *fses = (struct fake_session *)sess;

        if (fses->readable_addr_cstr)
        {
            return fses->readable_addr_cstr;
        }

        char ip_src_buf[INET6_ADDRSTRLEN] = {};
        char ip_dst_buf[INET6_ADDRSTRLEN] = {};
        char port_src_buf[16] = {};
        char port_dst_buf[16] = {};
        char tuple4_buf[256] = {};

        if (SESSION_ADDR_TYPE_IPV4_TCP == fses->addr_type)
        {
            inet_ntop(AF_INET, &fses->addr->ipv4.saddr, ip_src_buf, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &fses->addr->ipv4.daddr, ip_dst_buf, INET_ADDRSTRLEN);
            sprintf(port_src_buf, "%u", ntohs(fses->addr->ipv4.sport));
            sprintf(port_dst_buf, "%u", ntohs(fses->addr->ipv4.dport));
        }
        else
        {
            inet_ntop(AF_INET6, fses->addr->ipv6.saddr, ip_src_buf, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, fses->addr->ipv6.daddr, ip_dst_buf, INET6_ADDRSTRLEN);
            sprintf(port_src_buf, "%u", ntohs(fses->addr->ipv6.sport));
            sprintf(port_dst_buf, "%u", ntohs(fses->addr->ipv6.dport));
        }

        snprintf(tuple4_buf, sizeof(tuple4_buf), "%s.%s>%s.%s", ip_src_buf, port_src_buf, ip_dst_buf, port_dst_buf);
        fses->readable_addr_cstr = MMALLOC(char, strlen(tuple4_buf) + 1);
        memcpy(fses->readable_addr_cstr, tuple4_buf, strlen(tuple4_buf));

        return fses->readable_addr_cstr;
    }

    struct session_addr *session_get0_addr(struct session *sess, enum session_addr_type *addr_type)
    {
        struct fake_session *fses = (struct fake_session *)sess;
        *addr_type = fses->addr_type;
        return fses->addr;
    }

    static int __find_ex_data(struct fake_stellar *fst, const char *name)
    {
        int find_name_len = strlen(name);

        for (int i = 0; i < EX_DATA_MAX_SIZE; i++)
        {
            if ((fst->fake_exdata_mgr[i].name != NULL) && (strncasecmp(name, fst->fake_exdata_mgr[i].name, find_name_len) == 0) && (find_name_len == fst->fake_exdata_mgr[i].name_len))
            {
                return i;
            }
        }

        return -1;
    }

    static int __save_ex_data(struct fake_stellar *fst, const char *name, session_ex_free *free_func, void *arg)
    {
        for (int i = 0; i < EX_DATA_MAX_SIZE; i++)
        {
            if (fst->fake_exdata_mgr[i].name == NULL && fst->fake_exdata_mgr[i].name_len == 0)
            {
                fst->fake_exdata_mgr[i].name = MMALLOC(char, strlen(name) + 1);
                fst->fake_exdata_mgr[i].name_len = strlen(name) + 1;
                memcpy(fst->fake_exdata_mgr[i].name, name, strlen(name));
                fst->fake_exdata_mgr[i].free_func = free_func;
                fst->fake_exdata_mgr[i].arg = arg;
                return i;
            }
        }

        return -1;
    }

    int stellar_session_get_ex_new_index(struct stellar *st, const char *name, session_ex_free *free_func, void *arg)
    {
        int ex_id = __find_ex_data((struct fake_stellar *)st, name);
        if (-1 == ex_id)
        {
            ex_id = __save_ex_data((struct fake_stellar *)st, name, free_func, arg);
        }

        return ex_id;
    }

    int session_mq_get_topic_id(struct stellar *st, const char *topic_name)
    {
        return g_topic_id;
    }

    int session_get_current_thread_id(struct session *sess)
    {
        return 0;
    }

    int session_mq_destroy_topic(struct stellar *st, int topic_id)
    {
        return 0;
    }

    int session_set_ex_data(struct session *sess, int idx, void *ex_ptr)
    {
        struct fake_session *fses = (struct fake_session *)sess;
        fses->plug_exdata_array[idx] = ex_ptr;

        return 0;
    }

    void *session_get_ex_data(struct session *sess, int idx)
    {
        struct fake_session *fses = (struct fake_session *)sess;
        return fses->plug_exdata_array[idx];
    }

    void hdd_session_free_exdata(struct fake_session *fake_ses)
    {
        for (int i = 0; i < EX_DATA_MAX_SIZE; i++)
        {
            if (fake_ses->plug_exdata_array[i] != NULL)
            {
                fake_ses->fst->fake_exdata_mgr[i].free_func((struct session *)fake_ses, i, fake_ses->plug_exdata_array[i], NULL);
            }
        }
    }

    int stellar_plugin_register(struct stellar *st, int events, session_event_cb_func *cb, void *cb_arg)
    {
        return 0; // fix plugin id
    }

    extern int http_decoder_test_entry(struct session *sess, int topic_id, const void *data, void *cb_arg);
    int session_mq_publish_message(struct session *sess, int topic_id, void *data)
    {
        http_decoder_test_entry(sess, topic_id, data, NULL);

        g_msg_free_cb(data, g_msg_free_cb_arg);
        return 0;
    }

    int stellar_get_worker_thread_num(struct stellar *st)
    {
        return 1;
    }

    int session_mq_create_topic(struct stellar *st, const char *topic_name, msg_free_cb_func *free_cb, void *cb_arg)
    {
        g_msg_free_cb = free_cb;
        g_msg_free_cb_arg = cb_arg;

        g_topic_id = 0; // KISS, use fix value
        return g_topic_id;
    }

    const char *session_get0_current_payload(struct session *sess, size_t *payload_len)
    {
        struct fake_session *fses = (struct fake_session *)sess;
        struct fake_packet *fpkt = fses->fpkt;
        const char *payload_ptr = NULL;

        if (!fpkt || !fpkt->payload_data || fpkt->payload_data_len == 0)
        {
            return NULL;
        }

        int submit_len = MIN(fses->tcp_mss, fpkt->payload_data_len - fpkt->payload_submit_offset);
        if(submit_len <= 0)
        {
            *payload_len = 0;
            return NULL;
        }
        payload_ptr = fpkt->payload_data + fpkt->payload_submit_offset;
        *payload_len = submit_len;
        fpkt->payload_submit_offset += submit_len;

        return payload_ptr;
    }

    struct session_event *session_get_intrinsic_event(struct session *sess, int plugin_id)
    {
        return NULL;
    }

    int session_is_inner_most(struct session *sess, uint64_t *flag)
    {
        return 1; // no tunnel
    }

    int session_mq_subscribe_topic(struct stellar *st, int topic_id, on_msg_cb_func *sub_cb, void *cb_arg)
    {
        // to do
        return 0;
    }

#ifdef __cplusplus
}
#endif