#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "http_decoder_inc.h"
#include "llhttp.h"

struct http_tunnel_message
{
    enum http_tunnel_message_type type;
    hstring tunnel_payload;
};


int httpd_tunnel_identify(struct http_decoder_env *httpd_env, int curdir, struct http_decoder_half_data *hfdata)
{
    if(0 == httpd_env->hd_cfg.proxy_enable){
        return 0;
    }

    if(PACKET_DIRECTION_C2S == curdir){
        struct http_request_line reqline = {};
        http_decoder_half_data_get_request_line(hfdata, &reqline);
        if(0 == strncasecmp_safe("CONNECT", (char *)reqline.method.iov_base, 
                            7, reqline.method.iov_len)){
            return 1;
        }
    }else{
        struct http_response_line resline = {};
        http_decoder_half_data_get_response_line(hfdata, &resline);
        if(resline.status_code == HTTP_STATUS_OK
          && 0 == strncasecmp_safe("Connection established", (char *)resline.status.iov_base, 
                            strlen("Connection established"), resline.status.iov_len)){
            return 1;
        }        
    }

    return 0;
}

int httpd_is_tunnel_session(const struct http_decoder_exdata *ex_data)
{
    return (ex_data->tunnel_state != HTTP_TUN_NON);
}

int httpd_in_tunnel_transmitting(struct http_decoder_exdata *ex_data)
{
    return (ex_data->tunnel_state >= HTTP_TUN_INNER_STARTING);
}

enum http_tunnel_message_type httpd_tunnel_state_to_msg(const struct http_decoder_exdata *ex_data)
{
    if(ex_data->tunnel_state == HTTP_TUN_INNER_STARTING){
        return HTTP_TUNNEL_OPENING;
    }
    if(ex_data->tunnel_state == HTTP_TUN_INNER_TRANS){
        return HTTP_TUNNEL_ACTIVE;
    }
    return HTTP_TUNNEL_MSG_MAX;
}

void httpd_tunnel_state_update(struct http_decoder_exdata *ex_data)
{
    if(ex_data->tunnel_state == HTTP_TUN_INNER_STARTING){
        ex_data->tunnel_state = HTTP_TUN_INNER_TRANS;
    }
}

void http_decoder_push_tunnel_data(struct session *sess, const struct http_decoder_exdata *exdata, enum http_tunnel_message_type type)
{
    struct http_tunnel_message *tmsg = (struct http_tunnel_message *)CALLOC(struct http_tunnel_message, 1);
    tmsg->type = type;
    size_t payload_len;
    const char *payload = session_get0_current_payload(sess, &payload_len);
    tmsg->tunnel_payload.iov_base = (char *)payload;
    tmsg->tunnel_payload.iov_len = payload_len;
    session_mq_publish_message(sess, exdata->pub_topic_id, tmsg);
}

#ifdef __cplusplus
extern "C"
{
#endif
void http_tunnel_message_get_payload(const struct http_tunnel_message *tmsg,
                                     hstring *tunnel_payload)
{
    if (unlikely(NULL == tmsg || tunnel_payload == NULL))
    {
        return;
    }
    tunnel_payload->iov_base = tmsg->tunnel_payload.iov_base;
    tunnel_payload->iov_len = tmsg->tunnel_payload.iov_len;
}                                     

enum http_tunnel_message_type http_tunnel_message_type_get(const struct http_tunnel_message *tmsg)
{
    return tmsg->type;
}

#ifdef __cplusplus
}
#endif
