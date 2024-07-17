#include <gtest/gtest.h>
#include <unistd.h>
#include <stdio.h>
#include "http_decoder.h"
#include "http_decoder_inc.h"

void httpd_url_decode(const char *string, size_t length, char *ostring, size_t *olen);
TEST(url_decoder, none)
{
    char decode_url_buf[2048];
    size_t decode_url_len = sizeof(decode_url_buf);
    const char *encode_url = "https://docs.geedge.net/#all-updates";
    httpd_url_decode(encode_url, strlen(encode_url), decode_url_buf, &decode_url_len);

    EXPECT_STREQ("https://docs.geedge.net/#all-updates", decode_url_buf);
    EXPECT_EQ(decode_url_len, strlen("https://docs.geedge.net/#all-updates"));
}

TEST(url_decoder, simple)
{
    char decode_url_buf[2048];
    size_t decode_url_len = sizeof(decode_url_buf);
    const char *encode_url = "http://a.b.cn/%A1%B2%C3%D4";
    httpd_url_decode(encode_url, strlen(encode_url), decode_url_buf, &decode_url_len);

    const unsigned char expect_result[] = 
    {0x68, 0x74, 0x74, 0x70, 
    0x3A, 0x2F, 0x2F, 0x61, 
    0x2E, 0x62, 0x2E, 0x63, 
    0x6E, 0x2F, 0xA1, 0xB2, 
    0xC3, 0xD4, 0x00
    };
    EXPECT_STREQ((char *)expect_result, decode_url_buf);
    EXPECT_EQ(decode_url_len, sizeof(expect_result)-1);
}

TEST(url_decoder, chinese1)
{
    char decode_url_buf[2048];
    size_t decode_url_len = sizeof(decode_url_buf);
    const char *encode_url = "http://www.baidu.com/%E6%B5%8B%E8%AF%95%E4%B8%AD%E6%96%87%E8%A7%A3%E7%A0%81";
    httpd_url_decode(encode_url, strlen(encode_url), decode_url_buf, &decode_url_len);

    EXPECT_STREQ("http://www.baidu.com/\xE6\xB5\x8B\xE8\xAF\x95\xE4\xB8\xAD\xE6\x96\x87\xE8\xA7\xA3\xE7\xA0\x81", decode_url_buf);
    EXPECT_EQ(decode_url_len, strlen("http://www.baidu.com/\xE6\xB5\x8B\xE8\xAF\x95\xE4\xB8\xAD\xE6\x96\x87\xE8\xA7\xA3\xE7\xA0\x81"));
}

TEST(url_decoder, chinese2)
{
    char decode_url_buf[2048];
    size_t decode_url_len = sizeof(decode_url_buf);
    const char *encode_url = "http%3A%2F%2Fwww.baidu.com%2F%E7%BC%96%E8%A7%A3%E7%A0%81%E6%B5%8B%E8%AF%95%E5%93%88%E5%93%88";
    httpd_url_decode(encode_url, strlen(encode_url), decode_url_buf, &decode_url_len);

    EXPECT_STREQ("http://www.baidu.com/\xE7\xBC\x96\xE8\xA7\xA3\xE7\xA0\x81\xE6\xB5\x8B\xE8\xAF\x95\xE5\x93\x88\xE5\x93\x88", decode_url_buf);
    EXPECT_EQ(decode_url_len, strlen("http://www.baidu.com/\xE7\xBC\x96\xE8\xA7\xA3\xE7\xA0\x81\xE6\xB5\x8B\xE8\xAF\x95\xE5\x93\x88\xE5\x93\x88"));
}


int main(int argc, char const *argv[])
{
    ::testing::InitGoogleTest(&argc, (char **)argv);
    return RUN_ALL_TESTS();
}
