#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include "libnvram.h"

static int test_node(const struct nvram_node* node, const char* key, const char* value)
{
	if (!node) {
			printf("node not set\n");
			return 0;
		}
	if (!node->key) {
		printf("node->key not set\n");
		return 0;
	}
	if (strcmp(node->key, key)) {
		printf("node->key corrupt\n");
		return 0;
	}
	if (!node->value) {
		printf("node->value not set\n");
		return 0;
	}
	if (strcmp(node->value, value)) {
		printf("node->value corrupt\n");
		return 0;
	}

	return 1;
}

static int test_nvram_section()
{
	const uint8_t test_section1[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	const uint8_t test_section2[] = {
		0x5, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	uint32_t data_len_1 = 0;
	uint32_t data_len_2 = 0;
	uint32_t counter_1 = 1;
	uint32_t counter_2 = 2;

	if (is_valid_nvram_section((uint8_t*) &test_section1, sizeof(test_section1), &data_len_1, &counter_1)) {
		return 1;
	}

	if (counter_1 != 16) {
		printf("test_section1 wrong counter returned\n");
		return 1;
	}

	if (data_len_1 != 39) {
		printf("test_section1 wrong data_len returned\n");
		return 1;
	}

	if (is_valid_nvram_section((uint8_t*) &test_section2, sizeof(test_section2), &data_len_2, &counter_2)) {
		return 1;
	}

	if (counter_2 != 5) {
		printf("test_section2 wrong counter returned\n");
		return 1;
	}

	if (data_len_2 != 39) {
		printf("test_section2 wrong data_len returned\n");
		return 1;
	}

	return 0;
}

static int test_nvram_deserialize()
{
	const uint8_t test_section[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	const char* test_key1 = "TEST1";
	const char* test_val1 = "abcdefghij";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";

	int ret = 0;

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	ret = nvram_section_deserialize(&list, (uint8_t*) &test_section, sizeof(test_section));
	if (ret < 0) {
		printf("nvram_section_deserialize failed: %s\n", strerror(-ret));
		goto error_exit;
	}

	if(!test_node(list.entry, test_key1, test_val1)) {
		printf("node1 corrupt\n");
		goto error_exit;
	}

	if(!test_node(list.entry->next, test_key2, test_val2)) {
		printf("node2 corrupt\n");
		goto error_exit;
	}

	destroy_nvram_list(&list);
	return 1;

error_exit:
destroy_nvram_list(&list);
	return 0;
}

static int test_nvram_serialize()
{
	const uint8_t test_section[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_node test_node2 = {
		.key = "TEST2",
		.value = "def",
		.next = NULL,
	};

	struct nvram_node test_node1 = {
		.key = "TEST1",
		.value = "abcdefghij",
		.next = &test_node2,
	};

	int ret = 0;
	uint8_t* data = NULL;

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	list.entry = &test_node1;

	uint32_t size = 0;
	ret = nvram_section_serialize_size(&list, &size);
	if (ret < 0) {
		printf("nvram_section_serialize_size failed: %s\n", strerror(-ret));
		goto error_exit;
	}

	data = malloc(size);
	if (!data) {
		printf("malloc failed\n");
		goto error_exit;
	}

	ret = nvram_section_serialize(&list, 0x10, data, size);
	if (ret < 0) {
		printf("nvram_section_serialize failed: %s\n", strerror(-ret));
		goto error_exit;
	}

	if (sizeof(test_section) != size) {
		printf("serialized string wrong size %" PRIu32 " != %zu\n", size, sizeof(test_section));
		goto error_exit;
	}

	if (memcmp(data, test_section, size)) {
		printf("serialized string corrupt\n");
		goto error_exit;
	}

	if (data) {
		free(data);
	}
	return 1;

error_exit:
	if (data) {
		free(data);
	}
	return 0;
}

static int check_nvram_list_entry(const struct nvram_list* list, int index, const char* key, const char* value)
{
	struct nvram_node* cur = list->entry;
	for (int i = 0; cur; i++) {
		if (!strcmp(cur->key, key)) {
			if (!strcmp(cur->value, value)) {
				if (index != i) {
					printf("%s: %s: found at wrong index %d != %d\n", __func__, key, index, i);
					return 1;
				}
				return 0;
			}
			else {
				printf("%s: %s: value mismatch %s != %s\n", __func__, key, value, cur->value);
				return 1;
			}
		}
		cur = cur->next;
	}

	printf("%s: %s not found in list\n", __func__, key);
	return 1;
}

static int test_nvram_list_set()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";
	const char* test_key3 = "TEST3";
	const char* test_val3 = "ghi";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key3, test_val3);
	if (check_nvram_list_entry(&list, 2, test_key3, test_val3)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_overwrite_first()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_val2 = "def";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key1, test_val2);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val2)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_overwrite_second()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";
	const char* test_val3 = "ghi";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val3);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val3)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

struct test {
	char* name;
	int (*func)(void);
};

#define FUNC_NAME(NAME) #NAME

struct test test_array[] = {
		{FUNC_NAME(test_nvram_section), &test_nvram_section},
		{FUNC_NAME(test_nvram_deserialize), &test_nvram_deserialize},
		{FUNC_NAME(test_nvram_serialize), &test_nvram_serialize},
		{FUNC_NAME(test_nvram_list_set), &test_nvram_list_set},
		{FUNC_NAME(test_nvram_list_overwrite_first), &test_nvram_list_overwrite_first},
		{FUNC_NAME(test_nvram_list_overwrite_second), &test_nvram_list_overwrite_second},
		{NULL, NULL},
};

int main(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	int errors = 0;

	for (int i = 0; test_array[i].name; ++i) {
		int r = (*test_array[i].func)();
		if (r) {
			errors++;
		}
		printf("%s: %s\n", test_array[i].name, r ? "PASS" : "FAIL");
	}

	if(errors) {
		return 1;
	}
	return 0;
}
