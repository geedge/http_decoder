#include <string.h>
#include <assert.h>
#include "http_decoder.h"
#include "http_decoder_inc.h"

char *safe_dup(const char *str, size_t len)
{
    if (str == NULL || len == 0) {
        return NULL;
    }
    char *dup = CALLOC(char, len + 1);
    memcpy(dup, str, len);
    return dup;
}

int strncasecmp_safe(const char *fix_s1, const char *dyn_s2, size_t fix_n1, size_t dyn_n2)
{
    if (fix_s1 == NULL || dyn_s2 == NULL) {
        return -1;
    }
    if(fix_n1 != dyn_n2){
        return -1;
    }
    return strncasecmp(fix_s1, dyn_s2, fix_n1);
}

const char *http_message_type_to_string(enum http_message_type type)
{
    const char *sname = "unknown_msg_type";

    switch (type)
    {
    case HTTP_MESSAGE_REQ_LINE:
        sname = "HTTP_MESSAGE_REQ_LINE";
        break;
    case HTTP_MESSAGE_REQ_HEADER:
        sname = "HTTP_MESSAGE_REQ_HEADER";
        break;
    case HTTP_MESSAGE_REQ_HEADER_END:
        sname = "HTTP_MESSAGE_REQ_HEADER_END";
        break;
    case HTTP_MESSAGE_REQ_BODY_START:
        sname = "HTTP_MESSAGE_REQ_BODY_START";
        break;  
    case HTTP_MESSAGE_REQ_BODY: 
        sname = "HTTP_MESSAGE_REQ_BODY";
        break;
    case HTTP_MESSAGE_REQ_BODY_END:
        sname = "HTTP_MESSAGE_REQ_BODY_END";
        break;
    case HTTP_MESSAGE_RES_LINE:
        sname = "HTTP_MESSAGE_RES_LINE";
        break;  
    case HTTP_MESSAGE_RES_HEADER:       
        sname = "HTTP_MESSAGE_RES_HEADER";
        break;
    case HTTP_MESSAGE_RES_HEADER_END:
        sname = "HTTP_MESSAGE_RES_HEADER_END";
        break;
    case HTTP_MESSAGE_RES_BODY_START:
        sname = "HTTP_MESSAGE_RES_BODY_START";
        break;  
    case HTTP_MESSAGE_RES_BODY:
        sname = "HTTP_MESSAGE_RES_BODY";
        break;  
    case HTTP_MESSAGE_RES_BODY_END:
        sname = "HTTP_MESSAGE_RES_BODY_END";
        break;
    case HTTP_TRANSACTION_START:
        sname = "HTTP_TRANSACTION_START";
        break;
    case HTTP_TRANSACTION_END:
        sname = "HTTP_TRANSACTION_END";
        break;

    default:
        break;
    }

    return sname;
}

int http_message_type_is_req(struct session *sess, enum http_message_type msg_type)
{
    int is_req_msg = 0;

    switch(msg_type){
        case HTTP_MESSAGE_REQ_LINE:
        case HTTP_MESSAGE_REQ_HEADER:
        case HTTP_MESSAGE_REQ_HEADER_END:
        case HTTP_MESSAGE_REQ_BODY_START:
        case HTTP_MESSAGE_REQ_BODY:
        case HTTP_MESSAGE_REQ_BODY_END:
            is_req_msg = 1;
        break;

        case HTTP_MESSAGE_RES_LINE:
        case HTTP_MESSAGE_RES_HEADER:
        case HTTP_MESSAGE_RES_HEADER_END:
        case HTTP_MESSAGE_RES_BODY_START:
        case HTTP_MESSAGE_RES_BODY:
        case HTTP_MESSAGE_RES_BODY_END:
            is_req_msg = 0;
        break;

        case HTTP_TRANSACTION_START:
        case HTTP_TRANSACTION_END:
        {
            int cur_dir = packet_get_direction(session_get0_current_packet(sess));
            if(PACKET_DIRECTION_C2S == cur_dir){
                is_req_msg = 1;
            }else{
                is_req_msg = 0;
            }
        }
        break;

        default:
            assert(0);
            fprintf(stderr, "unknow message type:%d\n", (int)msg_type);
        break;
    }
    return is_req_msg;
}

int http_event_is_req(enum http_event event)
{
    switch(event){
        case HTTP_EVENT_REQ_INIT:
        case HTTP_EVENT_REQ_LINE:
        case HTTP_EVENT_REQ_HDR:
        case HTTP_EVENT_REQ_HDR_END:
        case HTTP_EVENT_REQ_BODY_BEGIN:
        case HTTP_EVENT_REQ_BODY_DATA:
        case HTTP_EVENT_REQ_BODY_END:
        case HTTP_EVENT_REQ_END:
            return 1;
        break;

        case HTTP_EVENT_RES_INIT:
        case HTTP_EVENT_RES_LINE:
        case HTTP_EVENT_RES_HDR:
        case HTTP_EVENT_RES_HDR_END:
        case HTTP_EVENT_RES_BODY_BEGIN:
        case HTTP_EVENT_RES_BODY_DATA:
        case HTTP_EVENT_RES_BODY_END:
        case HTTP_EVENT_RES_END:
            return 0;
        break;

        default:
            assert(0);
            fprintf(stderr, "unknow event type:%d\n", (int)event);
        break;
    }
    return -1;
}

int stellar_session_mq_get_topic_id_reliable(struct stellar *st, const char *topic_name, session_msg_free_cb_func *msg_free_cb, void *msg_free_arg)
{
    int topic_id = stellar_session_mq_get_topic_id(st, topic_name);
    if(topic_id < 0){
        topic_id = stellar_session_mq_create_topic(st, topic_name, msg_free_cb, msg_free_arg);
    }
    return topic_id;
}

static const unsigned char __g_httpd_hextable[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,       /* 0x30 - 0x3f */
  0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x4f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       /* 0x50 - 0x5f */
  0, 10, 11, 12, 13, 14, 15                                      /* 0x60 - 0x66 */
};

/* the input is a single hex digit */
#define onehex2dec(x) __g_httpd_hextable[x - '0']

#include <ctype.h>
// https://github.com/curl/curl/blob/2e930c333658725657b94a923d175c6622e0f41d/lib/urlapi.c
void httpd_url_decode(const char *string, size_t length, char *ostring, size_t *olen)
{
    size_t alloc = length;
    char *ns = ostring;

    while (alloc)
    {
        unsigned char in = (unsigned char)*string;
        if (('%' == in) && (alloc > 2) &&
            isxdigit(string[1]) && isxdigit(string[2]))
        {
            /* this is two hexadecimal digits following a '%' */
            in = (unsigned char)(onehex2dec(string[1]) << 4) | onehex2dec(string[2]);
            string += 3;
            alloc -= 3;
        }
        else
        {
            string++;
            alloc--;
        }
        *ns++ = (char)in;
    }
    *ns = 0; /* terminate it */

    if (olen)
        /* store output size */
        *olen = ns - ostring;

    return;
}

int httpd_url_is_encoded(const char *url, size_t len)
{
    for(size_t i = 0; i < len; i++){
        if(url[i] == '%'){
            return 1;
        }
    }
    return 0;
}