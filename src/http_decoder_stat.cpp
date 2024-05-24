#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "http_decoder_inc.h"

static __thread struct http_decoder_stat _th_stat;

int http_decoder_stat_init(struct http_decoder_context *ctx, int thread_num)
{
    ctx->fse = fieldstat_easy_new(thread_num, "http_decoder_statistics", NULL, 0);
    if (NULL == ctx->fse)
    {
        fprintf(stderr, "fieldstat_easy_new failed.");
        return -1;
    }

    ctx->fs_incoming_bytes_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_bytes");
    if (ctx->fs_incoming_bytes_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_bytes failed.");
        return -1;
    }

    ctx->fs_incoming_trans_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_trans");
    if (ctx->fs_incoming_trans_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_trans failed.");
        return -1;
    }

    ctx->fs_incoming_pkts_id =
        fieldstat_easy_register_counter(ctx->fse, "incoming_pkts");
    if (ctx->fs_incoming_pkts_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter incoming_pkts failed.");
        return -1;
    }

    ctx->fs_err_pkts_id = fieldstat_easy_register_counter(ctx->fse, "err_pkts");
    if (ctx->fs_err_pkts_id < 0)
    {
        fprintf(stderr, "fieldstat_easy_register_counter err_pkts failed.");
        return -1;
    }

    int stat_output_interval = DEFAULT_STAT_OUTPUT_INTERVAL;
    if (ctx->hd_cfg.stat_output_interval > 0)
    {
        stat_output_interval = ctx->hd_cfg.stat_output_interval;
    }

    int ret = fieldstat_easy_enable_auto_output(ctx->fse, FILEDSTAT_OUTPUT_FILE,
                                                stat_output_interval);
    if (ret < 0)
    {
        fprintf(stderr, "fieldstat_easy_enable_auto_output failed.");
        return -1;
    }

    return 0;
}

void http_decoder_stat_output(struct http_decoder_context *ctx, int thread_id)
{
    assert(ctx != NULL);

    int stat_interval_pkts = DEFAULT_STAT_INTERVAL_PKTS;
    if (ctx->hd_cfg.stat_interval_pkts > 0)
    {
        stat_interval_pkts = ctx->hd_cfg.stat_interval_pkts;
    }

    if (_th_stat.counter >= stat_interval_pkts)
    {
        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_incoming_bytes_id, NULL, 0,
                                      _th_stat.incoming_bytes);

        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_incoming_pkts_id, NULL, 0,
                                      _th_stat.incoming_pkts);

        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_incoming_trans_id, NULL, 0,
                                      _th_stat.incoming_trans);

        fieldstat_easy_counter_incrby(ctx->fse, thread_id,
                                      ctx->fs_err_pkts_id, NULL, 0,
                                      _th_stat.err_pkts);

        _th_stat.counter = 0;
        _th_stat.err_pkts = 0;
        _th_stat.incoming_bytes = 0;
        _th_stat.incoming_pkts = 0;
        _th_stat.incoming_trans = 0;
    }
}
