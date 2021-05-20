#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "libnvram.h"
#include "test-common.h"

static int test_libnvram_header_size()
{
	const uint32_t required = 24;
	if (libnvram_header_len() != required) {
		printf("libnvram_header_len != %u\n", required);
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_header()
{
	const uint8_t test_section[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x78, 0x97, 0xbf, 0xef, 0x04, 0xca, 0x55, 0x4d,
	};

	struct libnvram_header hdr;
	int r = libnvram_validate_header(test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("libnvram_validate_header: %d\n", r);
		return 1;
	}

	if (hdr.type != LIBNVRAM_TYPE_LIST) {
		printf("header wrong type returned\n");
		return 1;
	}

	if (hdr.user != 16) {
		printf("header wrong user returned\n");
		return 1;
	}

	if (hdr.len != 39) {
		printf("header wrong data_len returned\n");
		return 1;
	}

	if (hdr.crc32 != 0xefbf9778) {
		printf("header wrong data_crc32 returned\n");
		return 1;
	}

	if (hdr.hdr_crc32 != 0x4d55ca04) {
		printf("header wrong header_crc32 returned\n");
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_header_empty_data()
{
	const uint8_t test_section[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x03, 0x27, 0xb4, 0x55
	};

	struct libnvram_header hdr;
	int r = libnvram_validate_header(test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("libnvram_validate_header: %d\n", r);
		return 1;
	}

	if (hdr.type != LIBNVRAM_TYPE_LIST) {
		printf("header wrong type returned\n");
		return 1;
	}

	if (hdr.user != 0) {
		printf("header wrong user returned\n");
		return 1;
	}

	if (hdr.len != 0) {
		printf("header wrong data_len returned\n");
		return 1;
	}

	if (hdr.crc32 != 0) {
		printf("header wrong data_crc32 returned\n");
		return 1;
	}

	if (hdr.hdr_crc32 != 0x55b42703) {
		printf("header wrong header_crc32 returned\n");
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_header_corrupt_crc()
{
	const uint8_t test_section[] = {
			0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
			0x78, 0x97, 0xbf, 0xef, 0x01, 0x02, 0x03, 0x04,
	};

	struct libnvram_header hdr;
	int r = libnvram_validate_header(test_section, sizeof(test_section), &hdr);
	if (r != -LIBNVRAM_ERROR_CRC) {
		printf("libnvram_validate_header: no crc error\n");
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_header_wrong_magic()
{
	const uint8_t test_section[] = {
			0x01, 0x02, 0x03, 0x04, 0x10, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
			0x78, 0x97, 0xbf, 0xef, 0xcf, 0x7b, 0x28, 0x51,
	};

	struct libnvram_header hdr;
	int r = libnvram_validate_header(test_section, sizeof(test_section), &hdr);
	if (r != -LIBNVRAM_ERROR_INVALID) {
		printf("libnvram_validate_header: no invalid error\n");
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_data()
{
	struct libnvram_header hdr;
	hdr.user = 16;
	hdr.len = 39;
	hdr.crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	int r = libnvram_validate_data(test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("libnvram_validate_data: %d\n", r);
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_data_crc_corrupt()
{
	struct libnvram_header hdr;
	hdr.user = 16;
	hdr.len = 39;
	hdr.crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
			0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
			0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
			0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
			0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
			0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x67
	};

	int r = libnvram_validate_data(test_section, sizeof(test_section), &hdr);
	if (!r) {
		printf("libnvram_validate_data no error\n");
		return 1;
	}

	return 0;
}

static int test_libnvram_validate_data_entry_corrupt()
{
	struct libnvram_header hdr;
	hdr.user = 16;
	hdr.len = 39;
	hdr.crc32 = 0x5cc70915;

	const uint8_t test_section[] = {
			0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
			0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
			0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
			0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x54,
			0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	int r = libnvram_validate_data(test_section, sizeof(test_section), &hdr);
	if (!r) {
		printf("libnvram_validate_data no error\n");
		return 1;
	}

	return 0;
}

static int test_libnvram_deserialize()
{
	struct libnvram_header hdr;
	hdr.type = LIBNVRAM_TYPE_LIST;
	hdr.len = 39;

	const uint8_t test_section[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct libnvram_list *list = NULL;
	int r = libnvram_deserialize(&list, test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("libnvram_section_deserialize failed: %d\n", r);
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

	destroy_libnvram_list(&list);
	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_libnvram_deserialize_single()
{
	struct libnvram_header hdr;
	hdr.type = LIBNVRAM_TYPE_LIST;
	hdr.len = 25;

	const uint8_t test_section[] = {
		0x06, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x00, 0x61, 0x62,
		0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
		0x00
	};

	struct libnvram_entry entry1;
	const char* key = "TEST1";
	const char* value = "abcdefghij";
	entry1.key = (uint8_t*) key;
	entry1.key_len = strlen(key) + 1;
	entry1.value = (uint8_t*) value;
	entry1.value_len = strlen(value) + 1;

	struct libnvram_list *list = NULL;
	int r = libnvram_deserialize(&list, test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("libnvram_section_deserialize failed: %d\n", r);
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

	destroy_libnvram_list(&list);
	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_libnvram_deserialize_empty_data()
{
	struct libnvram_header hdr;
	hdr.type = LIBNVRAM_TYPE_LIST;
	hdr.len = 0;

	struct libnvram_list *list = NULL;
	const uint8_t test_section[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00
	};

	int r = libnvram_deserialize(&list, test_section, sizeof(test_section), &hdr);
	if (r) {
		printf("libnvram_section_deserialize failed: %d\n", r);
		goto error_exit;
	}

	if (list) {
		printf("list not empty\n");
		goto error_exit;
	}

	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_libnvram_deserialize_wrong_type()
{
	struct libnvram_header hdr;
	hdr.type = UINT8_MAX;
	hdr.len = 25;

	const uint8_t test_section[] = {
		0x06, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x00, 0x61, 0x62,
		0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
		0x00
	};

	struct libnvram_list *list = NULL;
	int r = libnvram_deserialize(&list, test_section, sizeof(test_section), &hdr);
	if (r != -LIBNVRAM_ERROR_INVALID) {
		printf("libnvram_section_deserialize failed: no invalid error\n");
		goto error_exit;
	}

	destroy_libnvram_list(&list);
	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_libnvram_serialize_size()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct libnvram_list *list = NULL;
	int r = libnvram_list_set(&list, &entry1);
	if (r) {
		printf("libnvram_list_set: %d\n", r);
		goto error_exit;
	}

	const uint32_t required = 47;
	uint32_t size = libnvram_serialize_size(list);
	if (size != required) {
		printf("returned %u != %u\n", size, required);
		goto error_exit;
	}

	destroy_libnvram_list(&list);

	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_libnvram_serialize_size_empty_data()
{
	struct libnvram_list *list = NULL;
	const uint32_t required = 24;
	uint32_t size = libnvram_serialize_size(list);
	if (size != required) {
		printf("returned %u != %u\n", size, required);
		goto error_exit;
	}

	return 0;

error_exit:
	return 1;
}

static int test_libnvram_serialize()
{
	struct libnvram_header hdr;
	hdr.user = 16;
	hdr.type = LIBNVRAM_TYPE_LIST;

	const uint8_t test_section[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x9b, 0xa2, 0x0a, 0x25,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct libnvram_list *list = NULL;
	int r = libnvram_list_set(&list, &entry1);
	if (r) {
		printf("libnvram_list_set failed: %d\n", r);
		goto error_exit;
	}
	r = libnvram_list_set(&list, &entry2);
	if (r) {
		printf("libnvram_list_set failed: %d\n", r);
		goto error_exit;
	}

	uint8_t buf[sizeof(test_section)];
	const uint32_t size = sizeof(test_section);
	const uint32_t bytes = libnvram_serialize(list, buf, size, &hdr);
	if (bytes != size) {
		printf("libnvram_serialize: returned %u != %u\n", bytes, size);
		goto error_exit;
	}

	if (hdr.len != 39) {
		printf("hdr.len: %u != %u\n", hdr.len, 39);
		goto error_exit;
	}

	if (hdr.user != 16) {
		printf("hdr.user: %u != %u\n", hdr.user, 16);
		goto error_exit;
	}

	if (hdr.crc32 != 0x6c9dd729) {
		printf("hdr.crc32: %04x != %04x\n", hdr.crc32, 0x6c9dd729);
		goto error_exit;
	}

	if (hdr.hdr_crc32 != 0x250aa29b) {
		printf("hdr.hdr_crc32: %04x != %04x\n", hdr.hdr_crc32, 0x250aa29b);
		goto error_exit;
	}

	if (memcmp(test_section, buf, size) != 0) {
		printf("buf != test_section\n");
		goto error_exit;
	}

	destroy_libnvram_list(&list);
	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_libnvram_serialize_empty_data()
{
	struct libnvram_header hdr;
	hdr.user = 16;
	hdr.type = LIBNVRAM_TYPE_LIST;

	const uint8_t test_section[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xcd, 0xef, 0x4d, 0xa7
	};

	struct libnvram_list *list = NULL;

	uint32_t size = sizeof(test_section);
	uint8_t buf[size];

	uint32_t bytes = libnvram_serialize(list, buf, size, &hdr);
	if (bytes != size) {
		printf("libnvram_serialize: returned %u != %u\n", bytes, size);
		goto error_exit;
	}

	if (hdr.len != 0) {
		printf("hdr.len: %u != %u\n", hdr.len, 0);
		goto error_exit;
	}

	if (hdr.user != 16) {
		printf("hdr.user: %u != %u\n", hdr.user, 16);
		goto error_exit;
	}

	if (hdr.crc32 != 0x00000000) {
		printf("hdr.crc32: %04x != %04x\n", hdr.crc32, 0x00000000);
		goto error_exit;
	}

	if (hdr.hdr_crc32 != 0xa74defcd) {
		printf("hdr.hdr_crc32: %04x != %04x\n", hdr.hdr_crc32, 0xa74defcd);
		goto error_exit;
	}

	if (memcmp(test_section, buf, size) != 0) {
		printf("buf != test_section\n");
		goto error_exit;
	}

	destroy_libnvram_list(&list);
	return 0;

error_exit:
	destroy_libnvram_list(&list);
	return 1;
}

static int test_iterator()
{
	struct libnvram_header hdr;
	hdr.user = 16;
	hdr.len = 39;
	hdr.crc32 = 0x6c9dd729;

	const uint8_t test_section[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	uint8_t *begin = libnvram_it_begin(test_section, sizeof(test_section), &hdr);
	if (begin != test_section) {
		printf("libnvram_it_begin not pointing to start of data\n");
		goto error_exit;
	}

	uint8_t *end = libnvram_it_end(test_section, sizeof(test_section), &hdr);
	if (end != test_section + sizeof(test_section)) {
		printf("libnvram_it_end not pointing to end of data\n");
		goto error_exit;
	}

	uint8_t* it = begin;
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abcdefghij");
	struct libnvram_entry entry;
	libnvram_it_deref(it, &entry);
	if (entrycmp(&entry, &entry1)) {
		printf("libnvram_it_deref: 1st entry wrong\n");
		goto error_exit;
	}

	it = libnvram_it_next(it);
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	libnvram_it_deref(it, &entry);
	if (entrycmp(&entry, &entry2)) {
		printf("libnvram_it_deref: 2nd entry wrong\n");
		goto error_exit;
	}

	it = libnvram_it_next(it);
	if (it != end) {
		printf("libnvram_it_next: did not end\n");
		goto error_exit;
	}

	return 0;

error_exit:
	return 1;
}

struct test test_array[] = {
		ADD_TEST(test_libnvram_header_size),
		ADD_TEST(test_libnvram_validate_header),
		ADD_TEST(test_libnvram_validate_header_empty_data),
		ADD_TEST(test_libnvram_validate_header_corrupt_crc),
		ADD_TEST(test_libnvram_validate_header_wrong_magic),
		ADD_TEST(test_libnvram_validate_data),
		ADD_TEST(test_libnvram_validate_data_crc_corrupt),
		ADD_TEST(test_libnvram_validate_data_entry_corrupt),
		ADD_TEST(test_libnvram_deserialize),
		ADD_TEST(test_libnvram_deserialize_single),
		ADD_TEST(test_libnvram_deserialize_empty_data),
		ADD_TEST(test_libnvram_deserialize_wrong_type),
		ADD_TEST(test_libnvram_serialize_size),
		ADD_TEST(test_libnvram_serialize_size_empty_data),
		ADD_TEST(test_libnvram_serialize),
		ADD_TEST(test_libnvram_serialize_empty_data),
		ADD_TEST(test_iterator),
		{NULL, NULL},
};
