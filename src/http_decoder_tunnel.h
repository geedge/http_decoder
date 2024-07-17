#pragma once 
#include "http_decoder_inc.h"
#include "http_decoder_half.h"

enum http_tunnel_state{
    HTTP_TUN_NON = 0,       // init, or not tunnel session
    HTTP_TUN_C2S_HDR_START, //CONNECT ...
    HTTP_TUN_C2S_END,       //CONNECT request end
    HTTP_TUN_S2C_START,     // HTTP 200 connet established
    HTTP_TUN_INNER_STARTING,   // http inner tunnel protocol starting
    HTTP_TUN_INNER_TRANS,   // http inner tunnel protocol transmitting 
};

/************************************************************
*       HTTP TUNNEL WITH CONNECT METHOD.
*************************************************************/
struct http_tunnel_message;
#define HTTP_DECODER_TUNNEL_TOPIC   "HTTP_DECODER_TUNNEL_MESSAGE"

enum http_tunnel_message_type {
    HTTP_TUNNEL_OPENING,
    HTTP_TUNNEL_ACTIVE,
    HTTP_TUNNEL_CLOSING,
    HTTP_TUNNEL_MSG_MAX
};
enum http_tunnel_message_type http_tunnel_message_type_get(const struct http_tunnel_message *tmsg);
void http_tunnel_message_get_payload(const struct http_tunnel_message *tmsg, struct iovec *tunnel_payload);


int httpd_tunnel_identify(struct http_decoder_env *httpd_env, int curdir, struct http_decoder_half_data *hfdata);
int httpd_is_tunnel_session(const struct http_decoder_exdata *ex_data);
int httpd_in_tunnel_transmitting(struct http_decoder_exdata *ex_data);
void httpd_tunnel_state_update(struct http_decoder_exdata *ex_data);
void http_decoder_push_tunnel_data(struct session *sess, const struct http_decoder_exdata *exdata, enum http_tunnel_message_type type);
enum http_tunnel_message_type httpd_tunnel_state_to_msg(const struct http_decoder_exdata *ex_data);