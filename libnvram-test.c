#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

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

static int test_nvram_header_size()
{
	if (nvram_header_len() != 16) {
		printf("nvram_header_len != 16\n");
		return 0;
	}

	return 1;
}

static int test_nvram_validate_header()
{
	const uint8_t test_section[] = {
		0x10, 0x00, 0x00, 0x00,
		0x27, 0x00, 0x00, 0x00,
		0x78, 0x97, 0xbf, 0xef,
		0x77, 0xbe, 0x15, 0x63,
	};

	struct nvram_header hdr;
	int r = nvram_validate_header(test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("nvram_validate_header: %s\n", strerror(-r));
		return 0;
	}

	if (hdr.counter != 16) {
		printf("header wrong counter returned\n");
		return 0;
	}

	if (hdr.data_len != 39) {
		printf("header wrong data_len returned\n");
		return 0;
	}

	if (hdr.data_crc32 != 0xefbf9778) {
		printf("header wrong data_crc32 returned\n");
		return 0;
	}

	if (hdr.header_crc32 != 0x6315be77) {
		printf("header wrong header_crc32 returned\n");
		return 0;
	}

	return 1;
}

static int test_nvram_validate_header_corrupt()
{
	const uint8_t test_section[] = {
		0x10, 0x00, 0x00, 0x00,
		0x27, 0x00, 0x00, 0x00,
		0x78, 0x97, 0xbf, 0xef,
		0x77, 0xbe, 0x15, 0x64,
	};

	struct nvram_header hdr;
	int r = nvram_validate_header(test_section, sizeof(test_section), &hdr);
	if (!r) {
		printf("nvram_validate_header no error\n");
		return 0;
	}

	return 1;
}

static int test_nvram_validate_data()
{
	struct nvram_header hdr;
	hdr.counter = 16;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	int r = nvram_validate_data(test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("nvram_validate_data: %s\n", strerror(-r));
		return 0;
	}

	return 1;
}

static int test_nvram_validate_data_crc_corrupt()
{
	struct nvram_header hdr;
	hdr.counter = 16;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
			0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
			0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
			0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
			0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
			0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x67
	};

	int r = nvram_validate_data(test_section, sizeof(test_section), &hdr);
	if (!r) {
		printf("nvram_validate_data no error\n");
		return 0;
	}

	return 1;
}

static int test_nvram_validate_data_entry_corrupt()
{
	struct nvram_header hdr;
	hdr.counter = 16;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x5cc70915;

	const uint8_t test_section[] = {
			0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
			0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
			0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
			0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x54,
			0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	int r = nvram_validate_data(test_section, sizeof(test_section), &hdr);
	if (!r) {
		printf("nvram_validate_data no error\n");
		return 0;
	}

	return 1;
}

static int test_nvram_deserialize()
{
	struct nvram_header hdr;
	hdr.counter = 16;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct nvram_list *list = NULL;
	int r = nvram_deserialize(&list, test_section, sizeof(test_section), &hdr);
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

static int test_nvram_serialize_size()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct nvram_list *list = NULL;
	int r = nvram_list_set(&list, &entry1);
	if (r) {
		printf("nvram_list_set: %s\n", strerror(-r));
		goto error_exit;
	}

	uint32_t size = nvram_serialize_size(list);
	if (size != 39) {
		printf("returned %u != 39\n", size);
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
	struct nvram_header hdr;
	hdr.counter = 16;
	//hdr.data_len = 39;
	//hdr.data_crc32 = 0x6c9dd729;
	//hdr.header_crc32 = 0x0b4ad6e8

	const uint8_t test_section[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c,	0xe8, 0xd6, 0x4a, 0x0b,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct nvram_list *list = NULL;
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

	uint32_t size = sizeof(test_section);
	uint8_t buf[sizeof(test_section)];

	uint32_t bytes = nvram_serialize(list, buf, size, &hdr);
	if (bytes != size) {
		printf("nvram_serialize: returned %u != %u\n", bytes, size);
		goto error_exit;
	}

	if (hdr.data_len != 39) {
		printf("hdr.data_len: %u != %u\n", hdr.data_len, size);
		goto error_exit;
	}

	if (hdr.counter != 16) {
		printf("hdr.counter: %u != %u\n", hdr.counter, 16);
		goto error_exit;
	}

	if (hdr.data_crc32 != 0x6c9dd729) {
		printf("hdr.data_crc32: %04x != %04x\n", hdr.data_crc32, 0x6c9dd729);
		goto error_exit;
	}

	if (hdr.header_crc32 != 0x0b4ad6e8) {
		printf("hdr.header_crc32: %04x != %04x\n", hdr.header_crc32, 0x0b4ad6e8);
		goto error_exit;
	}

	if (memcmp(test_section, buf, size)) {
		printf("buf != test_section\n");
		goto error_exit;
	}

	destroy_nvram_list(&list);
	return 1;

error_exit:
	destroy_nvram_list(&list);
	return 0;
}

static int test_iterator()
{
	struct nvram_header hdr;
	hdr.counter = 16;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	uint8_t *begin = nvram_it_begin(test_section, sizeof(test_section), &hdr);
	if (begin != test_section) {
		printf("nvram_it_begin not pointing to start of data\n");
		goto error_exit;
	}

	uint8_t *end = nvram_it_end(test_section, sizeof(test_section), &hdr);
	if (end != test_section + sizeof(test_section)) {
		printf("nvram_it_end not pointing to end of data\n");
		goto error_exit;
	}

	uint8_t* it = begin;
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct nvram_entry entry;
	nvram_it_deref(it, &entry);
	if (entrycmp(&entry, &entry1)) {
		printf("nvram_it_deref: 1st entry wrong\n");
		goto error_exit;
	}

	it = nvram_it_next(it);
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	nvram_it_deref(it, &entry);
	if (entrycmp(&entry, &entry2)) {
		printf("nvram_it_deref: 2nd entry wrong\n");
		goto error_exit;
	}

	it = nvram_it_next(it);
	if (it != end) {
		printf("nvram_it_next: did not end\n");
		goto error_exit;
	}

	return 1;

error_exit:
	return 0;
}


struct test {
	char* name;
	int (*func)(void);
};

#define FUNC_NAME(NAME) #NAME

struct test test_array[] = {
		{FUNC_NAME(test_nvram_header_size), &test_nvram_header_size},
		{FUNC_NAME(test_nvram_validate_header), &test_nvram_validate_header},
		{FUNC_NAME(test_nvram_validate_header_corrupt), &test_nvram_validate_header_corrupt},
		{FUNC_NAME(test_nvram_validate_data), &test_nvram_validate_data},
		{FUNC_NAME(test_nvram_validate_data_crc_corrupt), &test_nvram_validate_data_crc_corrupt},
		{FUNC_NAME(test_nvram_validate_data_entry_corrupt), &test_nvram_validate_data_entry_corrupt},
		{FUNC_NAME(test_nvram_deserialize), &test_nvram_deserialize},
		{FUNC_NAME(test_nvram_serialize_size), &test_nvram_serialize_size},
		{FUNC_NAME(test_nvram_serialize), &test_nvram_serialize},
		{FUNC_NAME(test_iterator), &test_iterator},
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
