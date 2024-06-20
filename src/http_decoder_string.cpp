#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "http_decoder_inc.h"

static const char *string_state_to_desc(enum string_state state)
{
    switch (state) {
    case STRING_STATE_INIT:
        return "init";
        break;
    case STRING_STATE_REFER:
        return "refer";
        break;
    case STRING_STATE_CACHE:
        return "cache";
        break;
    case STRING_STATE_COMMIT:
        return "commit";
        break;
    default:
        return "unknown";
        break;
    }
}

void http_decoder_string_refer(struct http_decoder_string *rstr,
                               const char *at, size_t length)
{
    if (NULL == rstr) {
        return;
    }

    switch (rstr->state) {
        case STRING_STATE_INIT:
        case STRING_STATE_CACHE:
            rstr->refer.iov_base = (char *)at;
            rstr->refer.iov_len = length;
            break;
        default:
            abort();
            break;
    }

    rstr->state = STRING_STATE_REFER;
}

static void string_refer2cache(struct http_decoder_string *rstr)
{
    if (0 == rstr->refer.iov_len) {
        return;
    }
    if (rstr->cache.iov_len >= rstr->max_cache_size) {
        return;
    }

    size_t length = rstr->cache.iov_len + rstr->refer.iov_len;
    if (length > rstr->max_cache_size) {
        length = rstr->max_cache_size;
    }

    if (NULL == rstr->cache.iov_base) {
        rstr->cache.iov_base = CALLOC(char, length + 1);
        memcpy(rstr->cache.iov_base, rstr->refer.iov_base, length);
    } else {
        rstr->cache.iov_base = REALLOC(char, rstr->cache.iov_base, length + 1);
        memcpy((char *)rstr->cache.iov_base + rstr->cache.iov_len, rstr->refer.iov_base,
               (length - rstr->cache.iov_len));
    }

    rstr->cache.iov_len = length;
    rstr->refer.iov_base = NULL;
    rstr->refer.iov_len = 0;
}

static void string_commit2cache(struct http_decoder_string *rstr)
{
    if (rstr->cache.iov_len == rstr->commit.iov_len &&
        rstr->cache.iov_base == rstr->commit.iov_base) {
        rstr->commit.iov_base = NULL;
        rstr->commit.iov_len = 0;
        return;
    }

    //Only http header key need to backward to cache
    size_t length = 0;
    if (rstr->commit.iov_len > rstr->max_cache_size) {
        length = rstr->max_cache_size;
    } else {
        length = rstr->commit.iov_len;
    }

    if (length > 0) {
        if (NULL == rstr->cache.iov_base) {
            rstr->cache.iov_base = CALLOC(char, length + 1);
        } else {
            abort();
        }
        memcpy(rstr->cache.iov_base, rstr->commit.iov_base, length);
        rstr->cache.iov_len = length;

        rstr->commit.iov_base = NULL;
        rstr->commit.iov_len = 0;
    }
}

void http_decoder_string_cache(struct http_decoder_string *rstr)
{
    if (NULL == rstr) {
        return;
    }

    switch (rstr->state) {
    case STRING_STATE_REFER:
        string_refer2cache(rstr);
        break;
    case STRING_STATE_CACHE:
        break;
    case STRING_STATE_COMMIT:
        //commit backward to cache
        string_commit2cache(rstr);
        break;
    default:
        abort();
        break;
    }
    rstr->state = STRING_STATE_CACHE;
}

void http_decoder_string_commit(struct http_decoder_string *rstr)
{
    if (NULL == rstr) {
        return;
    }

    switch (rstr->state) {
        case STRING_STATE_REFER:
            if (rstr->cache.iov_len) {
                http_decoder_string_cache(rstr);

                rstr->commit.iov_base = rstr->cache.iov_base;
                rstr->commit.iov_len = rstr->cache.iov_len;
                // not overwrite rstr->cache.iov_base
            } else {
                rstr->commit.iov_base = rstr->refer.iov_base;
                rstr->commit.iov_len = rstr->refer.iov_len;

                rstr->refer.iov_base = NULL;
                rstr->refer.iov_len = 0;
            }
            break;
        case STRING_STATE_CACHE:
            rstr->commit.iov_base = rstr->cache.iov_base;
            rstr->commit.iov_len = rstr->cache.iov_len;
            // not overwrite rstr->cache.iov_base
            break;
        default:
            //abort();
            break;
    }

    rstr->state = STRING_STATE_COMMIT;
}

void http_decoder_string_reset(struct http_decoder_string *rstr)
{
    assert(rstr);

    switch (rstr->state) {
    case STRING_STATE_INIT:
    case STRING_STATE_REFER:
    case STRING_STATE_CACHE:
    case STRING_STATE_COMMIT:
        FREE(rstr->cache.iov_base);
        memset(rstr, 0, sizeof(struct http_decoder_string));
        break;
    default:
        abort();
        break;
    }

    rstr->state = STRING_STATE_INIT;
}


void http_decoder_string_init(struct http_decoder_string *rstr,
                              size_t max_cache_size)
{
    rstr->max_cache_size = max_cache_size;
}

void http_decoder_string_reinit(struct http_decoder_string *rstr)
{
    if (rstr->state == STRING_STATE_CACHE) {
        return;
    }

    if (rstr->state == STRING_STATE_COMMIT &&
        rstr->cache.iov_base == rstr->commit.iov_base &&
        rstr->cache.iov_len == rstr->commit.iov_len) {
        return;
    }

    if (rstr->cache.iov_base != NULL) {
        FREE(rstr->cache.iov_base);
        rstr->cache.iov_len = 0;
    }

    rstr->refer.iov_base = NULL;
    rstr->refer.iov_len = 0;
    rstr->commit.iov_base = NULL;
    rstr->commit.iov_len = 0;
    rstr->state = STRING_STATE_INIT;
}

enum string_state http_decoder_string_state(const struct http_decoder_string *rstr)
{
    return rstr->state;
}

int http_decoder_string_get(const struct http_decoder_string *rstr, hstring *out)
{
    if (NULL == rstr || NULL == out) {
        return -1;
    }

    if (http_decoder_string_state(rstr) == STRING_STATE_COMMIT) {
        out->iov_base = rstr->commit.iov_base;
        out->iov_len = rstr->commit.iov_len;
    } else {
        out->iov_base = NULL;
        out->iov_len = 0;
    }

    return 0;
}

void http_decoder_string_dump(struct http_decoder_string *rstr, const char *desc)
{
    if (NULL == rstr) {
        return;
    }

    char *refer_str = safe_dup((char *)rstr->refer.iov_base, rstr->refer.iov_len);
    char *cache_str = safe_dup((char *)rstr->cache.iov_base, rstr->cache.iov_len);
    char *commit_str = safe_dup((char *)rstr->commit.iov_base, rstr->commit.iov_len);

    printf("%s: state: %s, refer: {len: %02zu, iov_base: %s}, cache: {len: %02zu, iov_base: %s}, commit: {len: %02zu, iov_base: %s}\n",
           desc, string_state_to_desc(rstr->state),
           rstr->refer.iov_len, refer_str,
           rstr->cache.iov_len, cache_str,
           rstr->commit.iov_len, commit_str);

    FREE(refer_str);
    FREE(cache_str);
    FREE(commit_str);
}