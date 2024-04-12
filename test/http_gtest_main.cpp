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
struct fake_stellar *g_fake_stellar; //export symbol for commit_test_result_json()
extern int http_decoder_entry(struct session *sess, int events,const struct packet *pkt, void *cb_arg);
extern int http_decoder_test_entry(struct session *sess, int topic_id, const void *data, void *cb_arg);
extern void *http_decoder_init(struct stellar *st);
extern void *http_decoder_test_init(struct stellar *st);
}
#endif

TEST(HTTP_DECODER, GTEST)
{
    ASSERT_EQ(0, hdgt_compare_result(g_fake_stellar));
}

int main(int argc, char **argv)
{
    g_fake_stellar = hdgt_create();
    
    if(hdgt_init_once(g_fake_stellar, argc, argv, http_decoder_init, http_decoder_entry, http_decoder_test_init, http_decoder_test_entry) < 0)
    {
        fprintf(stderr, "hdgt_init_once() fail!\n");
        exit(1);
    }

    hdgt_main_loop(g_fake_stellar);

    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();

    hdgt_destroy(g_fake_stellar);

    return ret;
}