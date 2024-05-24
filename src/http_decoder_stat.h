#ifndef _HTTP_DECODER_STAT_H_
#define _HTTP_DECODER_STAT_H_ 1
int http_decoder_stat_init(struct http_decoder_context *ctx, int thread_num);
void http_decoder_stat_output(struct http_decoder_context *ctx, int thread_id);
#endif