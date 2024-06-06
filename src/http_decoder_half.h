#ifndef _HTTP_DECODER_HALF_H_
#define _HTTP_DECODER_HALF_H_

#include <stddef.h>
#include "stellar/session.h"
#include "http_decoder.h"
#include "http_content_decompress.h"
#include "http_decoder_result_queue.h"
#include "llhttp.h"

// only one http event is fired at a time
enum http_event {
    HTTP_EVENT_REQ_INIT = 1 << 1,
    HTTP_EVENT_REQ_LINE = 1 << 2,
    HTTP_EVENT_REQ_HDR = 1 << 3,
    HTTP_EVENT_REQ_HDR_END = 1 << 4,
    HTTP_EVENT_REQ_BODY_BEGIN = 1 << 5,
    HTTP_EVENT_REQ_BODY_DATA = 1 << 6,
    HTTP_EVENT_REQ_BODY_END = 1 << 7,
    HTTP_EVENT_REQ_END = 1 << 8,

    HTTP_EVENT_RES_INIT = 1 << 9,
    HTTP_EVENT_RES_LINE = 1 << 10,
    HTTP_EVENT_RES_HDR = 1 << 11,
    HTTP_EVENT_RES_HDR_END = 1 << 12,
    HTTP_EVENT_RES_BODY_BEGIN = 1 << 13,
    HTTP_EVENT_RES_BODY_DATA = 1 << 14,
    HTTP_EVENT_RES_BODY_END = 1 << 15,
    HTTP_EVENT_RES_END = 1 << 16,
};

struct http_event_context {
    int topic_id;
    nmx_pool_t *ref_mempool;
    struct session *ref_session;
    struct http_decoder_result_queue *ref_queue;
};

struct http_decoder_half;
struct http_decoder_half_data;

typedef void http_event_cb(enum http_event event, struct http_decoder_half_data **data,
                           struct http_event_context *ev_ctx, void *httpd_plugin_env);

struct http_decoder_half *
http_decoder_half_new(nmx_pool_t *mempool, http_event_cb *event_cb, enum llhttp_type http_type,
                      int decompress_switch, struct http_decoder_env *httpd_env);

void http_decoder_half_free(nmx_pool_t *mempool, struct http_decoder_half *half);

void http_decoder_half_reinit(struct http_decoder_half *half, int topic_id,
                              struct http_decoder_result_queue *queue,
                              nmx_pool_t *mempool, struct session *sess);

int http_decoder_half_parse(struct http_decoder_half *half, const char *data, size_t data_len);

long long http_decoder_half_trans_count(struct http_decoder_half *half);

//http decoder half data API
struct http_decoder_half_data *
http_decoder_half_data_new(nmx_pool_t *mempool);

void http_decoder_half_data_free(nmx_pool_t *mempool, struct http_decoder_half_data *data);

int http_decoder_half_data_get_request_line(struct http_decoder_half_data *data,
                                            struct http_request_line *line);

int http_decoder_half_data_get_response_line(struct http_decoder_half_data *data,
                                             struct http_response_line *line);

int http_decoder_half_data_get_header(const struct http_decoder_half_data *data,
                                      const struct hstring *key, struct http_header *hdr_res);

int http_decoder_half_data_iter_header(struct http_decoder_half_data *data,
                                       struct http_header *header);
int http_decoder_half_data_reset_header_iter(struct http_decoder_half_data *req_data);
int http_decoder_half_data_has_parsed_header(struct http_decoder_half_data *data);

int http_decoder_half_data_get_raw_body(const struct http_decoder_half_data *data, struct hstring *body);

int http_decoder_half_data_get_decompress_body(const struct http_decoder_half_data *data, struct hstring *body);

void http_decoder_half_data_dump(struct http_decoder_half *half);

void http_decoder_get_host_feed_url(struct http_decoder_half *half);
void http_decoder_join_url(struct http_decoder_half_data *hfdata,
                           nmx_pool_t *mempool,
                           const struct http_header *host_hdr);
int http_decoder_join_url_finally(struct http_event_context *ev_ctx,
                                   struct http_decoder_half_data *hfdata,
                                   nmx_pool_t *mempool);
int http_half_data_get_url(struct http_decoder_half_data *res_data, struct hstring *url);
int http_half_data_get_transaction_seq(struct http_decoder_half_data *hf_data);

void http_half_data_update_commit_index(struct http_decoder_half_data * half_data);
void http_half_pre_context_free(struct session *sess, struct http_decoder_env *httpd_env,
                                   struct http_decoder_exdata *ex_data);
void http_half_update_state(struct http_decoder_half_data *hf_data, enum http_event state);      
int http_half_data_get_total_parsed_header_count(struct http_decoder_half_data * half_data);                             
#endif