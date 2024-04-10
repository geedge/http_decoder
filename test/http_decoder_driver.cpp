/*
    Http Decoder Google Test driver module
*/
#include <stdio.h>
#include <string.h>
#ifndef __USE_MISC
#define __USE_MISC 1
#endif
#ifndef __FAVOR_BSD
#define __FAVOR_BSD 1
#endif
#ifndef __USE_BSD
#define __USE_BSD 1
#endif
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <gtest/gtest.h>
#include <pcap/pcap.h>
#include "http_decoder_gtest.h"
#include "MESA_jump_layer.h"
#include "base64.h"

extern "C" int http_decoder_entry(struct session *sess, int events,
                                  const struct packet *pkt, void *cb_arg);
extern "C" void http_decoder_test_exit(void *test_ctx);
extern "C" void *http_decoder_test_init(struct stellar *st);
extern "C" void *http_decoder_init(struct stellar *st);
struct fake_stellar *g_fake_stellar; // used for plugin commit_test_result_json()

static const char *hdgt_cla_short_options = "hb:p:s:m:";
static const struct option hdgt_cla_long_options[] =
    {
        {"help", no_argument, NULL, 'h'},
        {"benchmark-file", required_argument, NULL, 'b'},
        {"data-pcap-file", required_argument, NULL, 'p'},
        {"data-json-file", required_argument, NULL, 's'},
        {"mss", required_argument, NULL, 'm'},
        {NULL, 0, NULL, 0}};

static const char *g_data_src_json_non_headers[] = {
    GTEST_HTTP_URL_NAME,
    GTEST_HTTP_TRANS_NAME,
    GTEST_HTTP_TRANS_SEQ_NAME,
    GTEST_HTTP_TUPLE4_NAME,
    GTEST_HTTP_PAYLOAD_NAME,
    GTEST_HTTP_PAYLOAD_MD5_NAME,
    "method",
    "uri",
    "req_version",
    "major_version",
    "minor_version",
    "res_version",
    "res_status",
    "status_code",
    NULL};

static void hdgt_cmd_usage(int argc, char **argv)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t -b set benchmark json file\n");
    fprintf(stderr, "\t -p set data source as pcap file\n");
    fprintf(stderr, "\t -s set data source as json file\n");
    fprintf(stderr, "\t -m set tcp Max Segment Size\n");
    exit(1);
}

static int hdgt_parse_cmd_args(fake_stellar *fst, int argc, char **argv)
{
    int c, ret;

    fst->tcp_mss = 1460; // set default value

    while (1)
    {
        c = getopt_long(argc, argv, hdgt_cla_short_options, hdgt_cla_long_options, NULL);
        if (c == -1)
        {
            ret = 0;
            break;
        }

        switch (c)
        {
        case 'h':
            hdgt_cmd_usage(argc, argv);
            break;
        case 'b':
            fst->benchmark_json_file_name = optarg;
            break;
        case 'p':
            fst->data_source_type = DATA_SOURCE_PCAP;
            fst->data_source_file_name = optarg;
            break;
        case 's':
            fst->data_source_type = DATA_SOURCE_JSON;
            fst->data_source_file_name = optarg;
            break;
        case 'm':
            fst->tcp_mss = atoi(optarg);
            if (fst->tcp_mss <= 0 || fst->tcp_mss > 65535)
            {
                DEBUG_PRINT("Invalid tcp mss value! must be [1, 65535]\n");
                hdgt_cmd_usage(argc, argv);
            }
            break;
        default:
            return -1;
            break;
        }
    }

    // check args
    if (!fst->benchmark_json_file_name)
    {
        DEBUG_PRINT("benchmark json file is not set!\n");
        hdgt_cmd_usage(argc, argv);
        return -1;
    }

    if (__DATA_SOURCE_NULL == fst->data_source_type)
    {
        DEBUG_PRINT("data source is not set!\n");
        hdgt_cmd_usage(argc, argv);
        return -1;
    }

    return ret;
}

static int hdgt_compare_result(struct fake_stellar *fst)
{
    int final_result = 0;
    cJSON_bool case_sensitive = FLASE;

    if (!fst->http_plug_test_result_root)
    {
        DEBUG_PRINT("Not found test json result!\n");
        return -1;
    }

    if (!fst->load_benchmark_json_root)
    {
        DEBUG_PRINT("Not found benchmark json instance!\n");
        return -1;
    }

    if (cJSON_GetArraySize(fst->load_benchmark_json_root) != cJSON_GetArraySize(fst->http_plug_test_result_root))
    {
        DEBUG_PRINT("Compare json result: array size is diff!\n");
        final_result++;
    }

    int ret = cJSON_Compare(fst->load_benchmark_json_root, fst->http_plug_test_result_root, case_sensitive);
    if (ret != TRUE)
    {
        char *load_json_str = cJSON_Print(fst->load_benchmark_json_root);
        printf("LOAD Raw:\n%s\n", load_json_str);

        char *result_json_str = cJSON_Print(fst->http_plug_test_result_root);
        printf("TEST Raw:\n%s\n", result_json_str);

        int min_len = MIN(strlen(load_json_str), strlen(result_json_str));
        for (size_t i = 0; i < min_len; i++)
        {
            if (load_json_str[i] != result_json_str[i])
            {
                printf("######### JSON Diff at len:%d: \n\tLOAD: %.*s\n\tTEST: %.*s\n", (int)i, 16, load_json_str + i, 16, result_json_str + i);
                break;
            }
        }

        free(load_json_str);
        free(result_json_str);

        cJSON *t_load = fst->load_benchmark_json_root->child;
        cJSON *t_test = fst->http_plug_test_result_root->child;
        while (t_load != NULL && t_test != NULL)
        {
            ret = cJSON_Compare(t_load, t_test, case_sensitive);
            if (ret != TRUE)
            {
                load_json_str = cJSON_Print(t_load);
                printf("LOAD Diff:\n%s\n", load_json_str);
                free(load_json_str);
                result_json_str = cJSON_Print(t_test);
                printf("TEST Diff:\n%s\n", result_json_str);
                free(result_json_str);
                final_result++;
            }
            t_load = t_load->next;
            t_test = t_test->next;
        }
    }
    else
    {
        DEBUG_PRINT("Compare json result success!\n");
    }

    return final_result;
}

static char *hdgt_get_file_content(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (NULL == fp)
    {
        DEBUG_PRINT("fopen() fail!\n");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *file_content = MMALLOC(char, file_size + 1);
    if (fread(file_content, 1, file_size, fp) != file_size)
    {
        DEBUG_PRINT("fread() %s fail!\n", filename);
        fclose(fp);
        MFREE(file_content);
        return NULL;
    }
    fclose(fp);
    return file_content;
}

static struct fake_session *hdgt_session_new(struct fake_stellar *fst)
{
    struct fake_session *fses = MMALLOC(struct fake_session, sizeof(struct fake_session));

    fses->fst = fst;
    fses->tcp_mss = fst->tcp_mss;
    // todo : get protocol type from packet, not fixed
    fses->type = SESSION_TYPE_TCP;
    return fses;
}

static int hdgt_get_packet_from_pcap(struct fake_stellar *fst, struct fake_packet *fpkt)
{
    struct pcap_pkthdr *pkt_hdr;
    const u_char *pkt_data;
    int ret = pcap_next_ex(fst->pcap_ins, &pkt_hdr, &pkt_data);
    if (1 == ret)
    {
        fpkt->raw_pkt_data = (char *)pkt_data;
        fpkt->raw_pkt_data_len = pkt_hdr->caplen;
        DEBUG_PRINT("Warning! this is a rough packet decoder, not support tcp out of order, seq overlap...\n");
        return 1;
    }
    else if (-2 == ret)
    {
        DEBUG_PRINT("pcap file over!\n");
        return 0;
    }
    DEBUG_PRINT("pcap_next_ex() fail: %s!\n", pcap_geterr(fst->pcap_ins));
    return -1;
}

static int hdgt_get_packet_from_json(struct fake_stellar *fst, struct fake_packet *fpkt)
{
    cJSON *json_root = fst->data_src_json_para.json_root;
    cJSON *json_item = cJSON_GetArrayItem(json_root, fst->data_src_json_para.current_json_array_idx);
    if (NULL == json_item)
    {
        DEBUG_PRINT("Not found json object at index %d!\n", fst->data_src_json_para.current_json_array_idx);
        return -1;
    }
    fst->data_src_json_para.current_object = json_item;
    fst->data_src_json_para.current_json_array_idx++;
    return 1;
}

static int hdgt_is_reserverd_json_header(const cJSON *json_object)
{
    for (size_t i = 0; g_data_src_json_non_headers[i] != NULL; i++)
    {
        if (strlen(json_object->string) == strlen(g_data_src_json_non_headers[i]) && (0 == strncmp(json_object->string, g_data_src_json_non_headers[i], strlen(g_data_src_json_non_headers[i]))))
        {
            return 1;
        }
    }

    return 0;
}

static int hdgt_get_packet_dir_from_json(cJSON *json)
{
    cJSON *json_dir = cJSON_GetObjectItem(json, GTEST_HTTP_TRANS_NAME);
    if (NULL == json_dir)
    {
        return -1;
    }
    if (strncasecmp("request", json_dir->valuestring, strlen("request")) == 0)
    {
        return PACKET_DIRECTION_C2S;
    }
    else if (strncasecmp("response", json_dir->valuestring, strlen("response")) == 0)
    {
        return PACKET_DIRECTION_S2C;
    }
    else
    {
        return -1;
    }
}

static void hdgt_get_req_line_from_json(struct data_src_json_para_t *data_src_json_para)
{
    cJSON *json_item = data_src_json_para->current_object;
    cJSON *method, *uri, *major_version, *minor_version;

    method = cJSON_GetObjectItem(json_item, "method");
    uri = cJSON_GetObjectItem(json_item, "uri");
    major_version = cJSON_GetObjectItem(json_item, "major_version");
    minor_version = cJSON_GetObjectItem(json_item, "minor_version");

    if (method && uri && major_version && minor_version)
    {
        sprintf(data_src_json_para->key_value_buf, "%s %s HTTP/%d.%d\r\n",
                method->valuestring, uri->valuestring, major_version->valueint, minor_version->valueint);
    }
    else
    {
        DEBUG_PRINT("get request line from json fail!\n");
    }
}

static void hdgt_get_res_line_from_json(struct data_src_json_para_t *data_src_json_para)
{
    cJSON *json_item = data_src_json_para->current_object;
    cJSON *res_status, *status_code, *major_version, *minor_version;

    res_status = cJSON_GetObjectItem(json_item, "res_status");
    status_code = cJSON_GetObjectItem(json_item, "status_code");
    major_version = cJSON_GetObjectItem(json_item, "major_version");
    minor_version = cJSON_GetObjectItem(json_item, "minor_version");

    if (res_status && status_code && major_version && minor_version)
    {
        sprintf(data_src_json_para->key_value_buf, "HTTP/%d.%d %d %s\r\n",
                major_version->valueint, minor_version->valueint, status_code->valueint, res_status->valuestring);
    }
    else
    {
        DEBUG_PRINT("get response line from json fail!\n");
    }
}

static void hdgt_get_headers_from_json(struct data_src_json_para_t *data_src_json_para)
{
    cJSON *json_item = data_src_json_para->current_object->child;
    char *data_ptr = data_src_json_para->key_value_buf + strlen(data_src_json_para->key_value_buf);
    int len;

    while (json_item)
    {
        if (0 == hdgt_is_reserverd_json_header(json_item))
        {
            if (cJSON_IsString(json_item))
            {
                len = sprintf(data_ptr, "%s: %s\r\n", json_item->string, json_item->valuestring);
            }
            else if (cJSON_IsNumber(json_item))
            {
                len = sprintf(data_ptr, "%s: %d\r\n", json_item->string, json_item->valueint);
            }
            else if (cJSON_IsBool(json_item))
            {
                len = sprintf(data_ptr, "%s: %s\r\n", json_item->string, json_item->valueint ? "true" : "false");
            }
            else
            {
                len = sprintf(data_ptr, "%s: %s\r\n", json_item->string, cJSON_Print(json_item));
            }
            data_ptr += len;
        }
        json_item = json_item->next;
    }
    sprintf(data_ptr, "\r\n"); // headers EOF
}

static void hdgt_get_body_from_json(struct data_src_json_para_t *data_src_json_para)
{
    cJSON *json_root = data_src_json_para->current_object;
    char *data_ptr = data_src_json_para->key_value_buf + strlen(data_src_json_para->key_value_buf);

    // todo, support gzip, deflate, etc.
    // todo, cjson not support binary data, need base64 encode/decode
    cJSON *json_body = cJSON_GetObjectItem(json_root, GTEST_HTTP_PAYLOAD_NAME);
    if (json_body)
    {
        int base64_cont_len = strlen(json_body->valuestring);
        char *raw_cont_buf = (char *)calloc(1, base64_cont_len + 1);
        int raw_cont_len = base64_decode(json_body->valuestring, base64_cont_len, (unsigned char *)raw_cont_buf);
        sprintf(data_ptr, "%s", raw_cont_buf);
    }
}

static int hdgt_update_packet_detail_by_json(struct fake_stellar *fst, struct fake_packet *fpkt)
{
    fpkt->dir = hdgt_get_packet_dir_from_json(fst->data_src_json_para.current_object);
    if (PACKET_DIRECTION_C2S != fpkt->dir && PACKET_DIRECTION_S2C != fpkt->dir)
    {
        DEBUG_PRINT("get packet direction from json fail!\n");
        return -1;
    }

    if (PACKET_DIRECTION_C2S == fpkt->dir)
    {
        hdgt_get_req_line_from_json(&fst->data_src_json_para);
    }
    else
    {
        hdgt_get_res_line_from_json(&fst->data_src_json_para);
    }
    hdgt_get_headers_from_json(&fst->data_src_json_para);

    hdgt_get_body_from_json(&fst->data_src_json_para);

    fpkt->payload_data = fst->data_src_json_para.key_value_buf;
    fpkt->payload_data_len = strlen(fst->data_src_json_para.key_value_buf);
    fpkt->payload_submit_offset = 0;

    return 0;
}

static int hdgt_get_packet(struct fake_stellar *fst, struct fake_packet *fpkt)
{
    int ret = -1;
    memset(fpkt, 0, sizeof(struct fake_packet));
    if (DATA_SOURCE_PCAP == fst->data_source_type)
    {
        ret = hdgt_get_packet_from_pcap(fst, fpkt);
    }
    else if (DATA_SOURCE_JSON == fst->data_source_type)
    {
        ret = hdgt_get_packet_from_json(fst, fpkt);
    }
    else
    {
        DEBUG_PRINT("Invalid data source type!\n");
        ret = -1;
    }

    return ret;
}

static int hdgt_has_data_left(struct fake_packet *fpkt)
{
    return fpkt->payload_submit_offset < fpkt->payload_data_len;
}

static int hdgt_get_packet_dir_of_tcp(const struct tcphdr *th)
{
    int dir;

    if (TH_SYN == th->th_flags)
    {
        dir = PACKET_DIRECTION_C2S;
    }
    else if ((TH_SYN | TH_ACK) == th->th_flags)
    {
        dir = PACKET_DIRECTION_S2C;
    }
    else
    {
        if (ntohs(th->th_sport) > ntohs(th->th_dport)) // Unwritten rule, not sure
        {
            dir = PACKET_DIRECTION_C2S;
        }
        else
        {
            dir = PACKET_DIRECTION_S2C;
        }
    }

    return dir;
}

static void hdgt_determine_packet_dir(struct fake_session *fses, struct fake_packet *fpkt, const struct tcphdr *th)
{
    if (SESSION_ADDR_TYPE_IPV4_TCP == fses->addr_type)
    {
        if (th->th_sport == fses->addr->ipv4.sport)
        {
            fpkt->dir = PACKET_DIRECTION_C2S;
        }
        else
        {
            fpkt->dir = PACKET_DIRECTION_S2C;
        }
    }
    else
    {
        if (th->th_sport == fses->addr->ipv6.sport)
        {
            fpkt->dir = PACKET_DIRECTION_C2S;
        }
        else
        {
            fpkt->dir = PACKET_DIRECTION_S2C;
        }
    }
}

static void hdgt_update_packet_payload(struct fake_packet *fpkt, const struct tcphdr *th, int from_tcp_len)
{
    // todo, support UDP?
    fpkt->payload_data = (char *)th + th->th_off * 4;
#if 0    
    /* for ethernet, maybe end with padding bytes, must not use packet length, use ip header len */
    fpkt->payload_data_len = fpkt->raw_pkt_data_len - ((char *)th - fpkt->raw_pkt_data) - (th->th_off * 4);
#else
    fpkt->payload_data_len = from_tcp_len - (th->th_off * 4);
#endif
    fpkt->payload_submit_offset = 0;
}

static int hdgt_update_packet_detail_by_pcap(struct fake_session *fses, struct fake_packet *fpkt)
{
    size_t from_tcp_len = 0;
    if (SESSION_ADDR_TYPE_IPV4_TCP == fses->addr_type)
    {
        struct ip *i4h = (struct ip *)MESA_jump_layer_greedy(fpkt->raw_pkt_data, ADDR_TYPE_MAC, ADDR_TYPE_IPV4);
        if (NULL == i4h)
        {
            DEBUG_PRINT("Not found ipv4 header!\n");
            return -1;
        }
        from_tcp_len = ntohs(i4h->ip_len) - i4h->ip_hl * 4;
    }
    else
    {
        struct ip6_hdr *i6h = (struct ip6_hdr *)MESA_jump_layer_greedy(fpkt->raw_pkt_data, ADDR_TYPE_MAC, ADDR_TYPE_IPV6);
        if (NULL == i6h)
        {
            DEBUG_PRINT("Not found ipv6 header!\n");
            return -1;
        }
        from_tcp_len = ntohs(i6h->ip6_plen);
    }

    struct tcphdr *th = (struct tcphdr *)MESA_jump_layer_greedy(fpkt->raw_pkt_data, ADDR_TYPE_MAC, ADDR_TYPE_TCP);
    if (NULL == th)
    {
        DEBUG_PRINT("Not found tcp header!\n");
        return -1;
    }

    hdgt_determine_packet_dir(fses, fpkt, th);
    hdgt_update_packet_payload(fpkt, th, from_tcp_len);
    return 0;
}

static int hdgt_update_packet_detail(struct fake_session *fses, struct fake_packet *fpkt)
{
    int ret;
    if (DATA_SOURCE_PCAP == fses->fst->data_source_type)
    {
        ret = hdgt_update_packet_detail_by_pcap(fses, fpkt);
    }
    else
    {
        ret = hdgt_update_packet_detail_by_json(fses->fst, fpkt);
    }
    return ret;
}

static struct session_addr *hgdt_get_session_addr_by_pcap(struct fake_session *fses, struct fake_packet *fpkt)
{
    if (fses->addr != NULL)
    {
        return fses->addr;
    }

    struct session_addr tmp_addr = {};
    int innermost_l3_layer_type;

    struct tcphdr *th = (struct tcphdr *)MESA_jump_layer_greedy(fpkt->raw_pkt_data, ADDR_TYPE_MAC, ADDR_TYPE_TCP);
    if (NULL == th)
    {
        DEBUG_PRINT("Not found tcp header!\n");
        // todo, support UDP ?
        return NULL;
    }
    fpkt->l4_header = (char *)th;
    struct ip *i4h = (struct ip *)MESA_jump_layer_greedy(fpkt->raw_pkt_data, ADDR_TYPE_MAC, ADDR_TYPE_IPV4);
    struct ip6_hdr *i6h = (struct ip6_hdr *)MESA_jump_layer_greedy(fpkt->raw_pkt_data, ADDR_TYPE_MAC, ADDR_TYPE_IPV6);
    if ((NULL == i6h) && (NULL == i4h))
    {
        DEBUG_PRINT("Not found ipv4 or ipv6 header!\n");
        return NULL;
    }

    if (i4h && (NULL == i6h))
    {
        innermost_l3_layer_type = ADDR_TYPE_IPV4;
    }
    else if (i6h && (NULL == i4h))
    {
        innermost_l3_layer_type = ADDR_TYPE_IPV6;
    }
    else // both ipv4 and ipv6 exist, maybe 4over6, 6over4, gtp etc.
    {
        if ((char *)i4h - (char *)i6h > 0)
        {
            innermost_l3_layer_type = ADDR_TYPE_IPV4;
        }
        else
        {
            innermost_l3_layer_type = ADDR_TYPE_IPV6;
        }
    }

    int cur_pkt_dir = hdgt_get_packet_dir_of_tcp(th);

    if (ADDR_TYPE_IPV4 == innermost_l3_layer_type)
    {
        fses->addr_type = SESSION_ADDR_TYPE_IPV4_TCP;
        if (PACKET_DIRECTION_C2S == cur_pkt_dir)
        {

            tmp_addr.ipv4.saddr = i4h->ip_src.s_addr;
            tmp_addr.ipv4.daddr = i4h->ip_dst.s_addr;
            tmp_addr.ipv4.sport = th->th_sport;
            tmp_addr.ipv4.dport = th->th_dport;
        }
        else
        {
            tmp_addr.ipv4.saddr = i4h->ip_dst.s_addr;
            tmp_addr.ipv4.daddr = i4h->ip_src.s_addr;
            tmp_addr.ipv4.sport = th->th_dport;
            tmp_addr.ipv4.dport = th->th_sport;
        }
    }
    else
    {
        fses->addr_type = SESSION_ADDR_TYPE_IPV6_TCP;
        if (PACKET_DIRECTION_C2S == cur_pkt_dir)
        {
            memcpy(tmp_addr.ipv6.saddr, i6h->ip6_src.s6_addr, IPV6_ADDR_LEN);
            memcpy(tmp_addr.ipv6.daddr, i6h->ip6_dst.s6_addr, IPV6_ADDR_LEN);
            tmp_addr.ipv6.sport = th->th_sport;
            tmp_addr.ipv6.dport = th->th_dport;
        }
        else
        {
            memcpy(tmp_addr.ipv6.saddr, i6h->ip6_dst.s6_addr, IPV6_ADDR_LEN);
            memcpy(tmp_addr.ipv6.daddr, i6h->ip6_src.s6_addr, IPV6_ADDR_LEN);
            tmp_addr.ipv6.sport = th->th_dport;
            tmp_addr.ipv6.dport = th->th_sport;
        }
    }

    fses->addr = MMALLOC(struct session_addr, sizeof(struct session_addr));
    memcpy(fses->addr, &tmp_addr, sizeof(struct session_addr));

    return fses->addr;
}

/* example: "1.1.1.1.12345>2.2.2.2.80" */
static void hdgt_json_session_adddr_pton_v4(struct session_addr *addr, const char *tuple4_cstr)
{
    const char *delim = ".>";
    char *dup_str = strdup(tuple4_cstr);
    char *sip1 = strtok(dup_str, delim);
    char *sip2 = strtok(NULL, delim);
    char *sip3 = strtok(NULL, delim);
    char *sip4 = strtok(NULL, delim);
    char *sport = strtok(NULL, delim);

    char *dip1 = strtok(NULL, delim);
    char *dip2 = strtok(NULL, delim);
    char *dip3 = strtok(NULL, delim);
    char *dip4 = strtok(NULL, delim);
    char *dport = strtok(NULL, delim);

    char sip_str[INET_ADDRSTRLEN] = {};
    char dip_str[INET_ADDRSTRLEN] = {};
    sprintf(sip_str, "%s.%s.%s.%s", sip1, sip2, sip3, sip4);
    inet_pton(AF_INET, sip_str, &addr->ipv4.saddr);
    sprintf(dip_str, "%s.%s.%s.%s", dip1, dip2, dip3, dip4);
    inet_pton(AF_INET, dip_str, &addr->ipv4.daddr);
    addr->ipv4.sport = htons(atoi(sport));
    addr->ipv4.dport = htons(atoi(dport));
}

/* fe80::8c19:7aff:fef2:11e5.12345>fe80::8c19:7aff:fef2:11e5.80 */
static void hdgt_json_session_adddr_pton_v6(struct session_addr *addr, const char *tuple4_cstr)
{
    char *dup_str = strdup(tuple4_cstr);
    char *sip = dup_str;
    char *ptr = strchr(dup_str, '.');
    *ptr = '\0';
    ptr++;
    char *sport = ptr;
    ptr = strchr(sport, '>');
    *ptr = '\0';
    ptr++;
    char *dip = ptr;
    ptr = strchr(dip, '.');
    *ptr = '\0';
    ptr++;
    char *dport = ptr;

    char sip_str[INET6_ADDRSTRLEN] = {};
    char dip_str[INET6_ADDRSTRLEN] = {};
    inet_pton(AF_INET6, sip, addr->ipv6.saddr);
    inet_pton(AF_INET6, dip, addr->ipv6.daddr);
    addr->ipv6.sport = htons(atoi(sport));
    addr->ipv6.dport = htons(atoi(dport));
}

static void hdgt_json_session_adddr_pton(struct fake_session *fses, const char *tuple4_cstr)
{
    struct session_addr *addr = fses->addr;
    if (strchr(tuple4_cstr, ':') != NULL)
    {
        hdgt_json_session_adddr_pton_v6(addr, tuple4_cstr);
        fses->addr_type = SESSION_ADDR_TYPE_IPV6_TCP;
        return;
    }
    else
    {
        hdgt_json_session_adddr_pton_v4(addr, tuple4_cstr);
        fses->addr_type = SESSION_ADDR_TYPE_IPV4_TCP;
    }
}

static struct session_addr *hgdt_get_session_addr_by_json(struct fake_session *fses, struct fake_packet *fpkt)
{
    if (fses->addr != NULL)
    {
        return fses->addr;
    }

    cJSON *tuple4_obj = cJSON_GetObjectItem(fses->fst->data_src_json_para.current_object, GTEST_HTTP_TUPLE4_NAME);
    if (NULL == tuple4_obj)
    {
        fprintf(stderr, "Not found tuple4 object!\n");
        return NULL;
    }
    fses->addr = MMALLOC(struct session_addr, sizeof(struct session_addr));
    hdgt_json_session_adddr_pton(fses, tuple4_obj->valuestring);
    return fses->addr;
}

static struct session_addr *hgdt_get_session_addr(struct fake_session *fses, struct fake_packet *fpkt)
{
    if (fses->fst->data_source_type == DATA_SOURCE_PCAP)
    {
        return hgdt_get_session_addr_by_pcap(fses, fpkt);
    }
    return hgdt_get_session_addr_by_json(fses, fpkt);
}

static void hdgt_session_update(struct fake_stellar *fst, struct fake_session *fses, int event)
{
    fst->http_decoder_entry((struct session *)fses, event, (struct packet *)fses->fpkt, fst->http_decoder_ctx);
}

static int hdgt_data_source_init(struct fake_stellar *fst)
{
    if (DATA_SOURCE_PCAP == fst->data_source_type)
    {
        char errbuf[PCAP_ERRBUF_SIZE];
        fst->pcap_ins = pcap_open_offline(fst->data_source_file_name, errbuf);
        if (NULL == fst->pcap_ins)
        {
            fprintf(stderr, "pcap_open_offline() fail: %s\n", errbuf);
            return -1;
        }
    }
    else if (DATA_SOURCE_JSON == fst->data_source_type)
    {
        char *file_cont = hdgt_get_file_content(fst->data_source_file_name);
        if (NULL == file_cont)
        {
            fprintf(stderr, "Open json file fail: %s\n", fst->data_source_file_name);
            return -1;
        }
        fst->data_src_json_para.json_root = cJSON_Parse(file_cont);
        if (NULL == fst->data_src_json_para.json_root)
        {
            fprintf(stderr, "cJSON_Parse() %s fail!\n", fst->data_source_file_name);
            MFREE(file_cont);
            return -1;
        }
        fst->data_src_json_para.json_array_size = cJSON_GetArraySize(fst->data_src_json_para.json_root);
        fst->data_src_json_para.current_json_array_idx = 0;
        MFREE(file_cont);
    }
    else
    {
        DEBUG_PRINT("Invalid data source type!\n");
        return -1;
    }

    return 0;
}

static void hdgt_prune_non_result_item(cJSON *benchmark_json_root)
{
    int array_size = cJSON_GetArraySize(benchmark_json_root);

    for (int i = 0; i < array_size; i++)
    {
        cJSON *object_root = cJSON_GetArrayItem(benchmark_json_root, i);
        cJSON_DeleteItemFromObject(object_root, GTEST_HTTP_PAYLOAD_NAME);
    }
}

static int hdgt_benchmakr_json_parse(struct fake_stellar *fst)
{
    char *file_cont = hdgt_get_file_content(fst->benchmark_json_file_name);
    if (NULL == file_cont)
    {
        fprintf(stderr, "Open json file fail: %s\n", fst->benchmark_json_file_name);
        return -1;
    }
    fst->load_benchmark_json_root = cJSON_Parse(file_cont);
    if (NULL == fst->load_benchmark_json_root)
    {
        fprintf(stderr, "cJSON_Parse() %s fail!\n", fst->benchmark_json_file_name);
        MFREE(file_cont);
        return -1;
    }
    fst->http_plug_test_result_root = cJSON_CreateArray();
    MFREE(file_cont);

    hdgt_prune_non_result_item(fst->load_benchmark_json_root);
    return 0;
}

static int hdgt_under_test_module_init(struct fake_stellar *fst)
{
    fst->http_decoder_ctx = http_decoder_init((struct stellar *)fst);
    fst->http_decoder_entry = http_decoder_entry;

    return 0;
}

static int hdgt_test_plug_init(struct fake_stellar *fst)
{
    fst->http_http_plug_ctx = http_decoder_test_init((struct stellar *)fst);
    return 0;
}

static struct fake_stellar *hdgt_init(int argc, char **argv)
{
    struct fake_stellar *fst = MMALLOC(struct fake_stellar, sizeof(struct fake_stellar));

    if (hdgt_parse_cmd_args(fst, argc, argv) < 0)
    {
        fprintf(stderr, "hdgt_parse_cmd_args() fail!\n");
        goto fail_exit;
    }
    if (hdgt_data_source_init(fst) < 0)
    {
        fprintf(stderr, "hdgt_data_source_init() fail!\n");
        goto fail_exit;
    }
    if (hdgt_benchmakr_json_parse(fst) < 0)
    {
        fprintf(stderr, "hdgt_benchmakr_json_parse() fail!\n");
        goto fail_exit;
    }
    if (hdgt_under_test_module_init(fst) < 0)
    {
        fprintf(stderr, "hdgt_under_test_module_init() fail!\n");
        goto fail_exit;
    }
    if (hdgt_test_plug_init(fst) < 0)
    {
        fprintf(stderr, "hdgt_test_plug_init() fail!\n");
        goto fail_exit;
    }

    g_fake_stellar = fst;
    return fst;

fail_exit:
    MFREE(fst);
    return NULL;
}

static void hdgt_exit(struct fake_stellar *fst)
{
    cJSON_free(fst->load_benchmark_json_root);
    cJSON_free(fst->http_plug_test_result_root);
    if (fst->data_src_json_para.json_root)
    {
        cJSON_free(fst->data_src_json_para.json_root);
    }
    if (fst->pcap_ins)
    {
        pcap_close(fst->pcap_ins);
    }

    MFREE(fst);
    return;
}

static void hdgt_session_free(struct fake_session *fses)
{
    const struct fake_exdata_manage *ex_mgr = fses->fst->fake_exdata_mgr;

    for (int i = 0; i < EX_DATA_MAX_SIZE; i++)
    {
        if (fses->plug_exdata_array[i] != NULL)
        {
            ex_mgr[i].free_func((struct session *)fses, i, fses->plug_exdata_array[i], ex_mgr[i].arg);
        }
    }

    if (fses->readable_addr_cstr)
    {
        MFREE(fses->readable_addr_cstr);
    }

    if (fses->addr)
    {
        MFREE(fses->addr);
    }

    MFREE(fses);
}

static void hdgt_main_loop(struct fake_stellar *fst)
{
    struct fake_packet __null_pkt = {};
    struct fake_packet fpkt = {};
    struct fake_session *fses = hdgt_session_new(fst);

    fses->fpkt = &__null_pkt;
    hdgt_session_update(fst, fses, SESS_EV_OPENING);

    fses->fpkt = &fpkt;
    while (hdgt_get_packet(fst, &fpkt) == 1)
    {
        if (NULL == hgdt_get_session_addr(fses, &fpkt))
        {
            continue; // not tcp, not ip
        }
        if (hdgt_update_packet_detail(fses, &fpkt) < 0)
        {
            continue;
        }
        while (hdgt_has_data_left(&fpkt))
        {
            hdgt_session_update(fst, fses, SESS_EV_PACKET);
        }
    }

    fses->fpkt = &__null_pkt;
    hdgt_session_update(fst, fses, SESS_EV_CLOSING);

    hdgt_session_free(fses);
}

TEST(HTTP_DECODER, GTEST)
{
    ASSERT_EQ(0, hdgt_compare_result(g_fake_stellar));
}

int main(int argc, char **argv)
{
    struct fake_stellar *fake_st = hdgt_init(argc, argv);
    if (NULL == fake_st)
    {
        fprintf(stderr, "hdgt_init() fail!\n");
        exit(1);
    }

    hdgt_main_loop(fake_st);

    ::testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();

    hdgt_exit(fake_st);

    return ret;
}
