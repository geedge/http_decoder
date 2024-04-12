#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <gtest/gtest.h>
#include "http_decoder_gtest.h"

#ifdef __cplusplus
extern "C"
{
    struct fake_stellar *g_fake_stellar; // export symbol for commit_test_result_json()
    extern int http_decoder_entry(struct session *sess, int events, const struct packet *pkt, void *cb_arg);
    extern int http_decoder_test_entry(struct session *sess, int topic_id, const void *data, void *cb_arg);
    extern void *http_decoder_init(struct stellar *st);
    extern void *http_decoder_test_init(struct stellar *st);
}
#endif

#define UNIT_TEST_SUCC_SSTRING "http unit test succ!"
#define UNIT_TEST_FAIL_SSTRING "http unit test fail!"

#define PRINT_TEST_RESULT(ret)                               \
    do                                                       \
    {                                                        \
        if (0 == ret)                                        \
        {                                                    \
            fprintf(stderr, "%s\n", UNIT_TEST_SUCC_SSTRING); \
            exit(200);                                       \
        }                                                    \
        else                                                 \
        {                                                    \
            fprintf(stderr, "%s\n", UNIT_TEST_FAIL_SSTRING); \
            exit(404);                                       \
        }                                                    \
    } while (0)

static int hgut_compare_hstring(const struct hstring *hs1, const struct hstring *hs2)
{
    if (hs1->str_len != hs2->str_len)
    {
        return -1;
    }
    if (strncmp(hs1->str, hs2->str, hs1->str_len) != 0)
    {
        return -1;
    }
    return 0;
}

static int hgut_compare_req_line(const struct http_request_line *req_line1, const struct http_request_line *req_line2)
{
    if (hgut_compare_hstring(&req_line1->method, &req_line2->method) != 0)
    {
        return -1;
    }
    if (hgut_compare_hstring(&req_line1->uri, &req_line2->uri) != 0)
    {
        return -1;
    }
    if (hgut_compare_hstring(&req_line1->version, &req_line2->version) != 0)
    {
        return -1;
    }
    if (req_line1->major_version != req_line2->major_version)
    {
        return -1;
    }
    if (req_line1->minor_version != req_line2->minor_version)
    {
        return -1;
    }

    return 0;
}

static int hgut_compare_res_line(const struct http_response_line *res_line1, const struct http_response_line *res_line2)
{
    if (hgut_compare_hstring(&res_line1->version, &res_line2->version) != 0)
    {
        return -1;
    }
    if (hgut_compare_hstring(&res_line1->status, &res_line2->status) != 0)
    {
        return -1;
    }
    if (res_line1->major_version != res_line2->major_version)
    {
        return -1;
    }
    if (res_line1->minor_version != res_line2->minor_version)
    {
        return -1;
    }
    if (res_line1->status_code != res_line2->status_code)
    {
        return -1;
    }

    return 0;
}

static int hgut_compare_header(const struct http_header *hdr1, const struct http_header *hdr2)
{
    if (hdr1->key.str_len != hdr2->key.str_len)
    {
        return -1;
    }
    if (hdr1->val.str_len != hdr2->val.str_len)
    {
        return -1;
    }
    if (strncmp(hdr1->key.str, hdr2->key.str, hdr1->key.str_len) != 0)
    {
        return -1;
    }
    if (strncmp(hdr1->val.str, hdr2->val.str, hdr1->val.str_len) != 0)
    {
        return -1;
    }

    return 0;
}

extern "C" int ut_get_req_line_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct http_request_line expect_req_line = {0};
    struct http_request_line req_line = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);

    expect_req_line.method.str = (char *)"GET";
    expect_req_line.method.str_len = 3;
    expect_req_line.uri.str = (char *)"/test_req_line/index.html";
    expect_req_line.uri.str_len = strlen("/test_req_line/index.html");
    expect_req_line.version.str = (char *)"1.1";
    expect_req_line.version.str_len = 3;
    expect_req_line.major_version = 1;
    expect_req_line.minor_version = 1;

    if (HTTP_MESSAGE_REQ_LINE == msg_type)
    {
        EXPECT_EQ(0, http_message_get_request_line(msg, &req_line));
        ret = hgut_compare_req_line(&expect_req_line, &req_line);
        PRINT_TEST_RESULT(ret);
    }

    return 0;
}

extern "C" int ut_get_url_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct hstring expect_url = {0};
    struct hstring get_url = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);

    expect_url.str = (char *)"get_url_test.com/test_url/index.html";
    expect_url.str_len = strlen(expect_url.str);

    if (HTTP_MESSAGE_REQ_LINE == msg_type)
    {
        EXPECT_EQ(-1, http_message_get_url(msg, &get_url));
    }

    if (HTTP_MESSAGE_REQ_HEADER == msg_type || HTTP_MESSAGE_REQ_BODY == msg_type || HTTP_MESSAGE_RES_LINE == msg_type || HTTP_MESSAGE_RES_HEADER == msg_type || HTTP_MESSAGE_RES_BODY == msg_type)
    {
        EXPECT_EQ(0, http_message_get_url(msg, &get_url));
        ret = hgut_compare_hstring(&expect_url, &get_url);
        if (ret != 0)
        {
            fprintf(stderr, "http_message_get_url value is diff! expect:%.*s, but actual:%.*s\n",
                    expect_url.str_len,
                    expect_url.str,
                    get_url.str_len,
                    get_url.str);
        }
        PRINT_TEST_RESULT(ret);
    }

    return 0;
}

extern "C" int ut_get_res_line_entry(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct http_response_line expect_res_line = {0};
    struct http_response_line res_line = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);

    expect_res_line.version.str = (char *)"1.1";
    expect_res_line.version.str_len = 3;
    expect_res_line.status.str = (char *)"OK";
    expect_res_line.status.str_len = 2;
    expect_res_line.status_code = 200;
    expect_res_line.major_version = 1;
    expect_res_line.minor_version = 1;

    if (HTTP_MESSAGE_REQ_LINE == msg_type || HTTP_MESSAGE_REQ_HEADER == msg_type || HTTP_MESSAGE_REQ_BODY == msg_type)
    {
        EXPECT_EQ(-1, http_message_get_response_line(msg, &res_line));
    }

    if (HTTP_MESSAGE_RES_LINE == msg_type)
    {
        EXPECT_EQ(0, http_message_get_response_line(msg, &res_line));
        ret = hgut_compare_res_line(&expect_res_line, &res_line);
        PRINT_TEST_RESULT(ret);
    }

    return 0;
}

extern "C" int ut_get_req_header(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct http_header expect_hdr = {0};
    struct http_header req_hdr = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);

    expect_hdr.key.str = (char *)"Host";
    expect_hdr.key.str_len = strlen("Host");
    expect_hdr.val.str = (char *)"192.168.40.137";
    expect_hdr.val.str_len = strlen("192.168.40.137");

    struct hstring fetch_key = expect_hdr.key;

    if (HTTP_MESSAGE_REQ_LINE == msg_type)
    {
        EXPECT_EQ(-1, http_message_get_header(msg, &fetch_key, &req_hdr));
    }

    if (HTTP_MESSAGE_REQ_HEADER == msg_type)
    {
        if (0 == http_message_get_header(msg, &fetch_key, &req_hdr))
        {
            ret = hgut_compare_header(&expect_hdr, &req_hdr);
            PRINT_TEST_RESULT(ret);
        }
    }

    return 0;
}

extern "C" int ut_get_res_header(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct http_header expect_hdr = {0};
    struct http_header req_hdr = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);

    expect_hdr.key.str = (char *)"Server";
    expect_hdr.key.str_len = strlen("Server");
    expect_hdr.val.str = (char *)"SimpleHTTP/0.6 Python/2.7.5";
    expect_hdr.val.str_len = strlen("SimpleHTTP/0.6 Python/2.7.5");

    struct hstring fetch_key = expect_hdr.key;

    if (HTTP_MESSAGE_REQ_LINE == msg_type || HTTP_MESSAGE_REQ_HEADER == msg_type || HTTP_MESSAGE_REQ_BODY == msg_type)
    {
        EXPECT_EQ(-1, http_message_get_header(msg, &fetch_key, &req_hdr));
    }

    if (HTTP_MESSAGE_RES_HEADER == msg_type)
    {
        if (0 == http_message_get_header(msg, &fetch_key, &req_hdr))
        {
            ret = hgut_compare_header(&expect_hdr, &req_hdr);
            PRINT_TEST_RESULT(ret);
        }
    }

    return 0;
}

extern "C" int ut_iterate_req_headers(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct http_header expect_hdr_array[16] = {0};
    struct http_header req_hdr_array[16] = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);
    static int hdr_count = 0;

    if (HTTP_MESSAGE_REQ_LINE == msg_type)
    {
        EXPECT_EQ(-1, http_message_header_next(msg, &req_hdr_array[0]));
    }

    expect_hdr_array[0].key.str = (char *)"Host";
    expect_hdr_array[0].key.str_len = strlen("Host");
    expect_hdr_array[0].val.str = (char *)"192.168.40.137";
    expect_hdr_array[0].val.str_len = strlen("192.168.40.137");

    expect_hdr_array[1].key.str = (char *)"User-Agent";
    expect_hdr_array[1].key.str_len = strlen("User-Agent");
    expect_hdr_array[1].val.str = (char *)"curl/7.79.1";
    expect_hdr_array[1].val.str_len = strlen("curl/7.79.1");

    expect_hdr_array[2].key.str = (char *)"Accept";
    expect_hdr_array[2].key.str_len = strlen("Accept");
    expect_hdr_array[2].val.str = (char *)"*/*";
    expect_hdr_array[2].val.str_len = strlen("*/*");

    if (HTTP_MESSAGE_REQ_HEADER == msg_type)
    {
        while (0 == http_message_header_next(msg, &req_hdr_array[hdr_count]))
        {
            ret = hgut_compare_header(&expect_hdr_array[hdr_count], &req_hdr_array[hdr_count]);
            if (0 != ret)
            {
                fprintf(stderr, "header value is diff!\n");
                PRINT_TEST_RESULT(ret);
            }
            hdr_count++;
        }

        hdr_count = 0;
        http_message_reset_header_iter(msg);
        // retry after reset iter

        while (0 == http_message_header_next(msg, &req_hdr_array[hdr_count]))
        {
            ret = hgut_compare_header(&expect_hdr_array[hdr_count], &req_hdr_array[hdr_count]);
            if (0 != ret)
            {
                fprintf(stderr, "header value is diff!\n");
                PRINT_TEST_RESULT(ret);
            }
            hdr_count++;
        }
    }

    if (HTTP_MESSAGE_REQ_BODY == msg_type || HTTP_MESSAGE_RES_LINE == msg_type || HTTP_MESSAGE_RES_HEADER == msg_type || HTTP_MESSAGE_RES_BODY == msg_type)
    {
        ret = hdr_count == 3 ? 0 : -1;
        PRINT_TEST_RESULT(ret);
    }

    return 0;
}

extern "C" int ut_iterate_res_headers(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct http_header expect_hdr_array[16] = {0};
    struct http_header res_hdr_array[16] = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);
    static int hdr_count = 0;

    if (HTTP_MESSAGE_REQ_LINE == msg_type || HTTP_MESSAGE_REQ_HEADER == msg_type || HTTP_MESSAGE_REQ_BODY == msg_type)
    {
        EXPECT_EQ(-1, http_message_header_next(msg, &res_hdr_array[0]));
    }

    expect_hdr_array[0].key.str = (char *)"Server";
    expect_hdr_array[0].key.str_len = strlen("Server");
    expect_hdr_array[0].val.str = (char *)"SimpleHTTP/0.6 Python/2.7.5";
    expect_hdr_array[0].val.str_len = strlen("SimpleHTTP/0.6 Python/2.7.5");

    expect_hdr_array[1].key.str = (char *)"Date";
    expect_hdr_array[1].key.str_len = strlen("Date");
    expect_hdr_array[1].val.str = (char *)"Thu, 30 Nov 2023 08:42:24 GMT";
    expect_hdr_array[1].val.str_len = strlen("Thu, 30 Nov 2023 08:42:24 GMT");

    expect_hdr_array[2].key.str = (char *)"Content-type";
    expect_hdr_array[2].key.str_len = strlen("Content-type");
    expect_hdr_array[2].val.str = (char *)"text/html";
    expect_hdr_array[2].val.str_len = strlen("text/html");

    expect_hdr_array[3].key.str = (char *)"Content-Length";
    expect_hdr_array[3].key.str_len = strlen("Content-Length");
    expect_hdr_array[3].val.str = (char *)"144";
    expect_hdr_array[3].val.str_len = strlen("144");

    expect_hdr_array[4].key.str = (char *)"Last-Modified";
    expect_hdr_array[4].key.str_len = strlen("Last-Modified");
    expect_hdr_array[4].val.str = (char *)"Thu, 30 Nov 2023 08:38:54 GMT";
    expect_hdr_array[4].val.str_len = strlen("Thu, 30 Nov 2023 08:38:54 GMT");

    if (HTTP_MESSAGE_RES_HEADER == msg_type)
    {
        while (0 == http_message_header_next(msg, &res_hdr_array[hdr_count]))
        {
            ret = hgut_compare_header(&expect_hdr_array[hdr_count], &res_hdr_array[hdr_count]);
            if (0 != ret)
            {
                fprintf(stderr, "header value is diff! expect:%.*s, but actual:%.*s\n",
                        expect_hdr_array[hdr_count].key.str_len,
                        expect_hdr_array[hdr_count].key.str,
                        res_hdr_array[hdr_count].key.str_len,
                        res_hdr_array[hdr_count].key.str);
                PRINT_TEST_RESULT(ret);
            }
            hdr_count++;
        }

        hdr_count = 0;
        http_message_reset_header_iter(msg);
        // retry after reset iter

        while (0 == http_message_header_next(msg, &res_hdr_array[hdr_count]))
        {
            ret = hgut_compare_header(&expect_hdr_array[hdr_count], &res_hdr_array[hdr_count]);
            if (0 != ret)
            {
                fprintf(stderr, "after reset iter, header value is diff! expect:%.*s, but actual:%.*s\n",
                        expect_hdr_array[hdr_count].key.str_len,
                        expect_hdr_array[hdr_count].key.str,
                        res_hdr_array[hdr_count].key.str_len,
                        res_hdr_array[hdr_count].key.str);
                PRINT_TEST_RESULT(ret);
            }
            hdr_count++;
        }

        ret = hdr_count == 5 ? 0 : -1;
        PRINT_TEST_RESULT(ret);
    }

    return 0;
}


extern "C" int ut_get_raw_body(struct session *sess, int topic_id, const void *data, void *cb_arg)
{
    int ret;
    struct hstring expect_body = {0};
    struct hstring get_body = {0};
    struct http_message *msg = (struct http_message *)data;
    enum http_message_type msg_type = http_message_type(msg);
    static int hdr_count = 0;

    /* 
        Raw body is html, including \r\n, is not easy to copy, so uses md5sum validation
    */
    expect_body.str = (char *)"3e11876cd3a234541ae37d833c088a76";
    expect_body.str_len = strlen(expect_body.str);

    if (HTTP_MESSAGE_REQ_LINE == msg_type || HTTP_MESSAGE_REQ_HEADER == msg_type || HTTP_MESSAGE_RES_LINE == msg_type || HTTP_MESSAGE_RES_HEADER == msg_type)
    {
        EXPECT_EQ(-1, http_message_get_raw_body(msg, &get_body));
    }

    if(HTTP_MESSAGE_RES_BODY == msg_type)
    {
        EXPECT_EQ(0, http_message_get_raw_body(msg, &get_body));
        char md5_result_string[33];
        MESA_MD5_sum_str((unsigned char *)get_body.str, (unsigned int)get_body.str_len, md5_result_string);
        ret = strcmp(expect_body.str, md5_result_string);
        PRINT_TEST_RESULT(ret);
    }
    
    return 0;
}

TEST(HTTP_DECODER_UNIT, GET_REQ_LINE)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/test_req_line/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_get_req_line_entry));
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, GET_URL)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/test_url/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"get_url_test.com\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_get_url_entry));
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, GET_RES_LINE)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"},{\"__X_HTTP_TRANSACTION\":\"response\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"res_version\":\"1.1\",\"res_status\":\"OK\",\"major_version\":1,\"minor_version\":1,\"status_code\":200,\"Server\":\"SimpleHTTP/0.6 Python/2.7.5\",\"Date\":\"Thu, 30 Nov 2023 08:42:24 GMT\",\"Content-type\":\"text/html\",\"Content-Length\":\"144\",\"Last-Modified\":\"Thu, 30 Nov 2023 08:38:54 GMT\",\"__X_HTTP_PAYLOAD_MD5\":\"3e11876cd3a234541ae37d833c088a76\",\"__X_HTTP_PAYLOAD\":\"PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KPHRpdGxlPlBhZ2UgVGl0bGU8L3RpdGxlPgo8L2hlYWQ+Cjxib2R5PgoKPGgxPlRoaXMgaXMgYSBIZWFkaW5nPC9oMT4KPHA+VGhpcyBpcyBhIHBhcmFncmFwaC48L3A+Cgo8L2JvZHk+CjwvaHRtbD4K\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_get_res_line_entry));
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, GET_REQ_HEADER)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"},{\"__X_HTTP_TRANSACTION\":\"response\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"res_version\":\"1.1\",\"res_status\":\"OK\",\"major_version\":1,\"minor_version\":1,\"status_code\":200,\"Server\":\"SimpleHTTP/0.6 Python/2.7.5\",\"Date\":\"Thu, 30 Nov 2023 08:42:24 GMT\",\"Content-type\":\"text/html\",\"Content-Length\":\"144\",\"Last-Modified\":\"Thu, 30 Nov 2023 08:38:54 GMT\",\"__X_HTTP_PAYLOAD_MD5\":\"3e11876cd3a234541ae37d833c088a76\",\"__X_HTTP_PAYLOAD\":\"PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KPHRpdGxlPlBhZ2UgVGl0bGU8L3RpdGxlPgo8L2hlYWQ+Cjxib2R5PgoKPGgxPlRoaXMgaXMgYSBIZWFkaW5nPC9oMT4KPHA+VGhpcyBpcyBhIHBhcmFncmFwaC48L3A+Cgo8L2JvZHk+CjwvaHRtbD4K\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_get_req_header));
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, GET_RES_HEADER)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"},{\"__X_HTTP_TRANSACTION\":\"response\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"res_version\":\"1.1\",\"res_status\":\"OK\",\"major_version\":1,\"minor_version\":1,\"status_code\":200,\"Server\":\"SimpleHTTP/0.6 Python/2.7.5\",\"Date\":\"Thu, 30 Nov 2023 08:42:24 GMT\",\"Content-type\":\"text/html\",\"Content-Length\":\"144\",\"Last-Modified\":\"Thu, 30 Nov 2023 08:38:54 GMT\",\"__X_HTTP_PAYLOAD_MD5\":\"3e11876cd3a234541ae37d833c088a76\",\"__X_HTTP_PAYLOAD\":\"PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KPHRpdGxlPlBhZ2UgVGl0bGU8L3RpdGxlPgo8L2hlYWQ+Cjxib2R5PgoKPGgxPlRoaXMgaXMgYSBIZWFkaW5nPC9oMT4KPHA+VGhpcyBpcyBhIHBhcmFncmFwaC48L3A+Cgo8L2JvZHk+CjwvaHRtbD4K\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_get_res_header));
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, ITERATE_REQ_HEADERS)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"},{\"__X_HTTP_TRANSACTION\":\"response\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"res_version\":\"1.1\",\"res_status\":\"OK\",\"major_version\":1,\"minor_version\":1,\"status_code\":200,\"Server\":\"SimpleHTTP/0.6 Python/2.7.5\",\"Date\":\"Thu, 30 Nov 2023 08:42:24 GMT\",\"Content-type\":\"text/html\",\"Content-Length\":\"144\",\"Last-Modified\":\"Thu, 30 Nov 2023 08:38:54 GMT\",\"__X_HTTP_PAYLOAD_MD5\":\"3e11876cd3a234541ae37d833c088a76\",\"__X_HTTP_PAYLOAD\":\"PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KPHRpdGxlPlBhZ2UgVGl0bGU8L3RpdGxlPgo8L2hlYWQ+Cjxib2R5PgoKPGgxPlRoaXMgaXMgYSBIZWFkaW5nPC9oMT4KPHA+VGhpcyBpcyBhIHBhcmFncmFwaC48L3A+Cgo8L2JvZHk+CjwvaHRtbD4K\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_iterate_req_headers));
    // hdgt_main_loop(fst);
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, ITERATE_RES_HEADERS)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"},{\"__X_HTTP_TRANSACTION\":\"response\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"res_version\":\"1.1\",\"res_status\":\"OK\",\"major_version\":1,\"minor_version\":1,\"status_code\":200,\"Server\":\"SimpleHTTP/0.6 Python/2.7.5\",\"Date\":\"Thu, 30 Nov 2023 08:42:24 GMT\",\"Content-type\":\"text/html\",\"Content-Length\":\"144\",\"Last-Modified\":\"Thu, 30 Nov 2023 08:38:54 GMT\",\"__X_HTTP_PAYLOAD_MD5\":\"3e11876cd3a234541ae37d833c088a76\",\"__X_HTTP_PAYLOAD\":\"PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KPHRpdGxlPlBhZ2UgVGl0bGU8L3RpdGxlPgo8L2hlYWQ+Cjxib2R5PgoKPGgxPlRoaXMgaXMgYSBIZWFkaW5nPC9oMT4KPHA+VGhpcyBpcyBhIHBhcmFncmFwaC48L3A+Cgo8L2JvZHk+CjwvaHRtbD4K\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_iterate_res_headers));
    // hdgt_main_loop(fst);
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

TEST(HTTP_DECODER_UNIT, GET_RAW_BODY)
{
    // go https://www.bejson.com to format JSON
    const char *test_json = "[{\"__X_HTTP_TUPLE4\":\"192.168.38.73.50806>192.168.40.137.80\"},{\"__X_HTTP_TRANSACTION\":\"request\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"method\":\"GET\",\"uri\":\"/index.html\",\"req_version\":\"1.1\",\"major_version\":1,\"minor_version\":1,\"Host\":\"192.168.40.137\",\"User-Agent\":\"curl/7.79.1\",\"Accept\":\"*/*\",\"__X_HTTP_URL\":\"192.168.40.137/index.html\"},{\"__X_HTTP_TRANSACTION\":\"response\",\"__X_HTTP_TRANSACTION_SEQ\":0,\"res_version\":\"1.1\",\"res_status\":\"OK\",\"major_version\":1,\"minor_version\":1,\"status_code\":200,\"Server\":\"SimpleHTTP/0.6 Python/2.7.5\",\"Date\":\"Thu, 30 Nov 2023 08:42:24 GMT\",\"Content-type\":\"text/html\",\"Content-Length\":\"144\",\"Last-Modified\":\"Thu, 30 Nov 2023 08:38:54 GMT\",\"__X_HTTP_PAYLOAD_MD5\":\"3e11876cd3a234541ae37d833c088a76\",\"__X_HTTP_PAYLOAD\":\"PCFET0NUWVBFIGh0bWw+CjxodG1sPgo8aGVhZD4KPHRpdGxlPlBhZ2UgVGl0bGU8L3RpdGxlPgo8L2hlYWQ+Cjxib2R5PgoKPGgxPlRoaXMgaXMgYSBIZWFkaW5nPC9oMT4KPHA+VGhpcyBpcyBhIHBhcmFncmFwaC48L3A+Cgo8L2JvZHk+CjwvaHRtbD4K\"}]";
    struct fake_stellar *fst = hdgt_create();
    hdgt_set_data_source_type(fst, DATA_SOURCE_JSON);
    EXPECT_EQ(0, hdgt_data_source_init_by_json_text(fst, test_json));
    EXPECT_EQ(0, hdgt_under_test_module_init(fst, http_decoder_init, http_decoder_entry));
    EXPECT_EQ(0, hdgt_test_plug_init(fst, http_decoder_test_init, ut_get_raw_body));
    // hdgt_main_loop(fst);
    EXPECT_EXIT(hdgt_main_loop(fst), testing::ExitedWithCode(200), UNIT_TEST_SUCC_SSTRING);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    int ret = RUN_ALL_TESTS();
    return ret;
}