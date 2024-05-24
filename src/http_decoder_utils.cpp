#include <string.h>
#include <assert.h>
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
    case HTTP_MESSAGE_RES_BODY:
        sname = "HTTP_MESSAGE_RES_BODY";
        break;  
    case HTTP_MESSAGE_RES_BODY_END:
        sname = "HTTP_MESSAGE_RES_BODY_END";
        break;
    case HTTP_TRANSACTION_NEW:
        sname = "HTTP_TRANSACTION_NEW";
        break;
    case HTTP_TRANSACTION_FREE:
        sname = "HTTP_TRANSACTION_FREE";
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
        case HTTP_MESSAGE_REQ_BODY:
        case HTTP_MESSAGE_REQ_BODY_END:
            is_req_msg = 1;
        break;

        case HTTP_MESSAGE_RES_LINE:
        case HTTP_MESSAGE_RES_HEADER:
        case HTTP_MESSAGE_RES_HEADER_END:
        case HTTP_MESSAGE_RES_BODY:
        case HTTP_MESSAGE_RES_BODY_END:
            is_req_msg = 0;
        break;

        case HTTP_TRANSACTION_NEW:
        case HTTP_TRANSACTION_FREE:
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