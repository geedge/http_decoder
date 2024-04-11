#include "stream.h"
#include "cJSON.h"
#include <gtest/gtest.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "http_decoder_gtest.h"

// #define IGNORE_PRINTF
#ifdef IGNORE_PRINTF
#define printf(fmt, ...) (0)
#endif

cJSON *g_test_result_root = NULL;
cJSON *g_load_result_root = NULL;
int g_ret = 0;


extern "C" int libsapp_setup_env(int argc, char *argv[]);

extern "C" int commit_test_result_json(cJSON *node, const char *name)
{
	if (g_test_result_root)
	{
		// cJSON_AddItemToObject(g_test_result_root, name, node);
		// cJSON_AddStringToObject(node, "name", name);
		cJSON_AddItemToArray(g_test_result_root, node);
		return 0;
	}
	return -1;
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

static cJSON *load_result_from_jsonfile(const char *json_path)
{
	if (json_path == NULL)
		return NULL;

	long file_len = 0;
	char *file_content = NULL;
	FILE *fp = NULL;

	fp = fopen(json_path, "r+");
	if (NULL == fp)
	{
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (file_len == 0)
	{
		fclose(fp);
		return NULL;
	}
	file_content = (char *)malloc(file_len + 1);
	fread(file_content, file_len, 1, fp);
	file_content[file_len] = '\0';
	cJSON *load = cJSON_Parse(file_content);
	free(file_content);
	fclose(fp);

	hdgt_prune_non_result_item(load);

	return load;
}

TEST(PROTOCOL, compare_result_json)
{
	EXPECT_EQ(cJSON_GetArraySize(g_test_result_root), cJSON_GetArraySize(g_load_result_root));
	int ret = cJSON_Compare(g_test_result_root, g_load_result_root, 0);
	EXPECT_EQ(1, ret);

	if (ret != 1)
	{
		char *load_json_str = cJSON_Print(g_load_result_root);
		printf("LOAD Raw:\n%s\n", load_json_str);
		free(load_json_str);
		char *result_json_str = cJSON_Print(g_test_result_root);
		printf("TEST Raw:\n%s\n", result_json_str);
		free(result_json_str);

		cJSON *t_load = g_load_result_root->child, *t_test = g_test_result_root->child;
		while (t_load != NULL)
		{
			ret = cJSON_Compare(t_load, t_test, 0);
			if (ret != 1)
			{
				load_json_str = cJSON_Print(t_load);
				printf("LOAD Diff:\n%s\n", load_json_str);
				free(load_json_str);
				result_json_str = cJSON_Print(t_test);
				printf("TEST Diff:\n%s\n", result_json_str);
				free(result_json_str);
				ASSERT_EQ(1, ret);
			}
			t_load = t_load->next;
			t_test = t_test->next;
		}
	}
}

void run_gtest_main()
{
	g_ret = RUN_ALL_TESTS();
	exit(g_ret);
}

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		printf("Invalid Argument!!!\n Usage: ./test_protocol_plug_main [/path/to/result_json] [sapp_args...]\n");
		return -1;
	}
	const char *result_json_path = argv[1];
	g_test_result_root = cJSON_CreateArray();
	g_load_result_root = load_result_from_jsonfile(result_json_path);
	assert(g_load_result_root != NULL && g_test_result_root != NULL);
	::testing::InitGoogleTest(&argc, argv);

	if (libsapp_setup_env(argc - 1, argv + 1) < 0)
	{
		return -1;
	}
	atexit(run_gtest_main);
	while (1)
		pause();
	return 0;
}
