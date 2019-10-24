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
static uint8_t test_serialized[] = {
										  0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
										  0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
									      0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
										  0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66};
static uint8_t test_section1[] = {
										  0x01, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
										  0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
										  0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
									      0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
										  0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66};

static uint8_t test_section2[] = {
										  0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0xf3, 0x06, 0xec, 0xfd, \
										  0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
										  0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
									      0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
										  0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66};

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

static int test_deserialize()
{
	struct nvram_node* node = NULL;
	int ret = 0;

	ret = nvram_deserialize(&node, (uint8_t*) &test_serialized, sizeof(test_serialized));
	if (ret < 0) {
		printf("Deserialize failed: %s\n", strerror(-ret));
		goto error_exit;
	}

	if(!test_node(node, test_key1, test_val1)) {
		printf("node1 corrupt\n");
		goto error_exit;
	}

	node = node->next;
	if(!test_node(node, test_key2, test_val2)) {
		printf("node2 corrupt\n");
		goto error_exit;
	}

	destroy_nvram_node(node);
	return 1;

error_exit:
	destroy_nvram_node(node);
	return 0;
}

static int test_serialize()
{
	int ret = 0;
	uint8_t* data = NULL;
	uint32_t size = 0;

	ret = nvram_serialize(&test_node1, &data, &size);
	if (ret) {
		printf("Serialize failed: %s\n", strerror(-ret));
		goto error_exit;
	}

	if (sizeof(test_serialized) != size) {
		printf("serialized string wrong size %" PRIu32 " != %zu\n", size, sizeof(test_serialized));
		goto error_exit;
	}

	if (memcmp(data, test_serialized, size)) {
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

static int test_nvram_section()
{
	uint32_t data_len = 0;
	if (is_valid_nvram_section((uint8_t*) &test_section1, sizeof(test_section1), &data_len)) {
		return 1;
	}

	if (is_valid_nvram_section((uint8_t*) &test_section2, sizeof(test_section2), &data_len)) {
		return 1;
	}



	return 0;
}

int main(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	if (test_deserialize()) {
		printf("test_deserialize: PASS\n");
	}
	else {
		printf("test_deserialize: FAIL\n");
	}

	if (test_serialize()) {
		printf("test_serialize: PASS\n");
	}
	else {
		printf("test_serialize: FAIL\n");
	}

	if (test_nvram_section()) {
		printf("test_nvram_section: PASS\n");
	}
	else {
		printf("test_nvram_section: FAIL\n");
	}

	return 0;
}
