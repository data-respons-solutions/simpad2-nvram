#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include "libnvram.h"


static const char* test_key1 = "TEST1";
static const char* test_val1 = "abcdefghij";
static const char* test_key2 = "TEST2";
static const char* test_val2 = "def";

static struct nvram_node test_node2 = {
	.key = "TEST2",
	.value = "def",
	.next = NULL,
};
static struct nvram_node test_node1 = {
	.key = "TEST1",
	.value = "abcdefghij",
	.next = &test_node2,
};

static uint8_t test_section1[] = {
										  0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
										  0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
										  0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
									      0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
										  0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66};

static uint8_t test_section2[] = {
										  0x5, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
										  0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
										  0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
									      0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
										  0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66};


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

	int ret = 0;

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	ret = nvram_section_deserialize(&list, (uint8_t*) &test_section1, sizeof(test_section1));
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

	if (sizeof(test_section1) != size) {
		printf("serialized string wrong size %" PRIu32 " != %zu\n", size, sizeof(test_section1));
		goto error_exit;
	}

	if (memcmp(data, test_section1, size)) {
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

int main(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	if (test_nvram_section()) {
		printf("test_nvram_section: PASS\n");
	}
	else {
		printf("test_nvram_section: FAIL\n");
	}

	if (test_nvram_deserialize()) {
		printf("test_nvram_deserialize: PASS\n");
	}
	else {
		printf("test_nvram_deserialize: FAIL\n");
	}

	if (test_nvram_serialize()) {
		printf("test_nvram_serialize: PASS\n");
	}
	else {
		printf("test_nvram_serialize: FAIL\n");
	}

	return 0;
}
