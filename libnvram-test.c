#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include "libnvram.h"

// return 0 for equal
static int keycmp(const uint8_t* key1, uint32_t key1_len, const uint8_t* key2, uint32_t key2_len)
{
	if (key1_len == key2_len) {
		return memcmp(key1, key2, key1_len);
	}
	return 1;
}

// return 0 for equal
static int entrycmp(const struct nvram_entry* entry1, const struct nvram_entry* entry2)
{
	if (!keycmp(entry1->key, entry1->key_len, entry2->key, entry2->key_len)) {
		return keycmp(entry1->value, entry1->value_len, entry2->value, entry2->value_len);
	}
	return 1;
}

static void fill_entry(struct nvram_entry* entry, const char* key, const char* value)
{
	entry->key = (uint8_t*) key;
	entry->key_len = strlen(key);
	entry->value = (uint8_t*) value;
	entry->value_len = strlen(value);
}

static int test_nvram_section()
{
	const uint8_t test_section1[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x78, 0x97, 0xbf, 0xef, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	const uint8_t test_section2[] = {
		0x5, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x78, 0x97, 0xbf, 0xef, \
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
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x78, 0x97, 0xbf, 0xef, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct nvram_list *list = NULL;
	int r = nvram_section_deserialize(&list, (uint8_t*) &test_section, sizeof(test_section));
	if (r) {
		printf("nvram_section_deserialize failed: %s\n", strerror(-r));
		goto error_exit;
	}

	if (!list) {
		printf("list empty\n");
		goto error_exit;
	}

	if (entrycmp(list->entry, &entry1)) {
		printf("entry1 wrong\n");
		goto error_exit;
	}

	if (entrycmp(list->next->entry, &entry2)) {
		printf("entry2 wrong\n");
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
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x78, 0x97, 0xbf, 0xef, \
		0x5, 0x0, 0x0, 0x0, 0xa, 0x0, 0x0, 0x0, 0x54, 0x45, 0x53, \
		0x54, 0x31, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, \
		0x69, 0x6a, 0x5, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x54, \
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct nvram_list *list = NULL;
	uint8_t* data = NULL;
	uint32_t size = 0;
	int r = nvram_list_set(&list, &entry1);
	if (r) {
		printf("nvram_list_set failed: %s\n", strerror(-r));
		goto error_exit;
	}
	r = nvram_list_set(&list, &entry2);
	if (r) {
		printf("nvram_list_set failed: %s\n", strerror(-r));
		goto error_exit;
	}

	r = nvram_section_serialize_size(list, &size);
	if (r) {
		printf("nvram_section_serialize_size failed: %s\n", strerror(-r));
		goto error_exit;
	}

	data = malloc(size);
	if (!data) {
		printf("malloc failed\n");
		goto error_exit;
	}

	r = nvram_section_serialize(list, 0x10, data, size);
	if (r) {
		printf("nvram_section_serialize failed: %s\n", strerror(-r));
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
	destroy_nvram_list(&list);
	return 1;

error_exit:
	if (data) {
		free(data);
	}
	destroy_nvram_list(&list);
	return 0;
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
		{NULL, NULL},
};

int main(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	int errors = 0;

	for (int i = 0; test_array[i].name; ++i) {
		int r = (*test_array[i].func)();
		if (!r) {
			errors++;
		}
		printf("%s: %s\n", test_array[i].name, r ? "PASS" : "FAIL");
	}

	printf("Result: %s\n", errors ? "FAIL" : "PASS");

	if(errors) {
		return 1;
	}
	return 0;
}
