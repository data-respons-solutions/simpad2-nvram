#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "libnvram.h"
#include "test-common.h"

static int test_nvram_init_transaction()
{
	struct nvram_header hdr1;
	hdr1.counter = 16;
	hdr1.data_len = 39;
	hdr1.data_crc32 = 0x6c9dd729;
	hdr1.header_crc32 = 0x0b4ad6e8;
	/*
	%10%00%00%00%27%00%00%00%29%d7%9d%6c => 0x0b4ad6e8
	*/
	const uint8_t data1[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0xe8, 0xd6, 0x4a, 0x0b,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};


	struct nvram_header hdr2;
	hdr2.counter = 10;
	hdr2.data_len = 39;
	hdr2.data_crc32 = 0x6c9dd729;
	hdr2.header_crc32 = 0x447e38b4;
	/*
	%0a%00%00%00%27%00%00%00%29%d7%9d%6c => 0x447e38b4
	*/

	const uint8_t data2[] = {
		0x0a, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0xb4, 0x38, 0x7e, 0x44,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_transaction trans;

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, data1, sizeof(data1), data2, sizeof(data2));
	if (trans.active != NVRAM_ACTIVE_A) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_a.hdr, &hdr1, sizeof(hdr1))) {
		printf("header1 wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr2, sizeof(hdr2))) {
		printf("header2 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, data2, sizeof(data2), data1, sizeof(data1));
	if (trans.active != NVRAM_ACTIVE_B) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_a.hdr, &hdr2, sizeof(hdr2))) {
		printf("header1 wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr1, sizeof(hdr1))) {
		printf("header2 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, data1, sizeof(data1), data1, sizeof(data1));
	if (trans.active != (NVRAM_ACTIVE_A | NVRAM_ACTIVE_B)) {
		printf("Wrong section active\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_nvram_init_transaction_corrupt_header()
{
	struct nvram_header hdr;
	hdr.counter = 16;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x6c9dd729;
	hdr.header_crc32 = 0x0b4ad6e8;
	const uint8_t data1[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0xe8, 0xd6, 0x4a, 0x0b,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	/*
	struct nvram_header corrupt;
	hdr.counter = 10;
	hdr.data_len = 39;
	hdr.data_crc32 = 0x6c9dd729;
	hdr.header_crc32 = 0x607e38b4;
	*/
	const uint8_t data2[] = {
		0x0a, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0xb4, 0x38, 0x7e, 0x60,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct nvram_transaction trans;

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, data2, sizeof(data2), data1, sizeof(data1));
	if (trans.active != NVRAM_ACTIVE_B) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr, sizeof(hdr))) {
		printf("header2 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != NVRAM_STATE_HEADER_CORRUPT) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, data1, sizeof(data1), data2, sizeof(data2));
	if (trans.active != NVRAM_ACTIVE_A) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_a.hdr, &hdr, sizeof(hdr))) {
		printf("header1 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != NVRAM_STATE_HEADER_CORRUPT) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}
static int test_nvram_init_transaction_corrupt_data()
{
	struct nvram_header hdr1;
	hdr1.counter = 16;
	hdr1.data_len = 39;
	hdr1.data_crc32 = 0x6c9dd729;
	hdr1.header_crc32 = 0x0b4ad6e8;
	/*
	%10%00%00%00%27%00%00%00%29%d7%9d%6c => 0x0b4ad6e8
	*/
	const uint8_t data1[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0xe8, 0xd6, 0x4a, 0x0b,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};


	struct nvram_header hdr2;
	hdr2.counter = 10;
	hdr2.data_len = 39;
	hdr2.data_crc32 = 0x6c9dd729;
	hdr2.header_crc32 = 0x447e38b4;
	/*
	%0a%00%00%00%27%00%00%00%29%d7%9d%6c => 0x447e38b4
	*/

	const uint8_t corrupt[] = {
		0x0a, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0xb4, 0x38, 0x7e, 0x44,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x67
	};

	struct nvram_transaction trans;

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, data1, sizeof(data1), corrupt, sizeof(corrupt));
	if (trans.active != NVRAM_ACTIVE_A) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_a.hdr, &hdr1, sizeof(hdr1))) {
		printf("header1 wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr2, sizeof(hdr2))) {
		printf("header2 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != (NVRAM_STATE_HEADER_VERIFIED | NVRAM_STATE_DATA_CORRUPT)) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	nvram_init_transaction(&trans, corrupt, sizeof(corrupt), data1, sizeof(data1));
	if (trans.active != NVRAM_ACTIVE_B) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_a.hdr, &hdr2, sizeof(hdr2))) {
		printf("header1 wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr1, sizeof(hdr1))) {
		printf("header2 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != (NVRAM_STATE_HEADER_VERIFIED | NVRAM_STATE_DATA_CORRUPT)) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != NVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_nvram_next_transaction()
{
	struct nvram_transaction trans;
	memset(&trans, 0, sizeof(trans));

	trans.section_a.hdr.counter = 16;
	trans.section_a.state = NVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.counter = 10;
	trans.section_b.state = NVRAM_STATE_ALL_VERIFIED;
	trans.active = NVRAM_ACTIVE_A;

	struct nvram_header new;
	enum nvram_operation op = nvram_next_transaction(&trans, &new);
	if (op != NVRAM_OPERATION_WRITE_B) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.counter != 17) {
		printf("returned counter %u is not %u\n", new.counter, 17);
		goto error_exit;
	}

	trans.section_a.hdr.counter = 10;
	trans.section_a.state = NVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.counter = 6;
	trans.section_b.state = NVRAM_STATE_ALL_VERIFIED;
	trans.active = NVRAM_ACTIVE_B;

	op = nvram_next_transaction(&trans, &new);
	if (op != NVRAM_OPERATION_WRITE_A) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.counter != 7) {
		printf("returned counter %u is not %u\n", new.counter, 7);
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_nvram_next_transaction_new()
{
	struct nvram_transaction trans;
	memset(&trans, 0, sizeof(trans));

	trans.section_a.state = NVRAM_STATE_HEADER_CORRUPT;
	trans.section_b.state = NVRAM_STATE_HEADER_CORRUPT;
	trans.active = NVRAM_ACTIVE_NONE;

	struct nvram_header new;
	enum nvram_operation op = nvram_next_transaction(&trans, &new);
	if (op != NVRAM_OPERATION_WRITE_A) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.counter != 1) {
		printf("returned counter %u is not %u\n", new.counter, 1);
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_nvram_next_transaction_counter_reset()
{
	struct nvram_transaction trans;
	memset(&trans, 0, sizeof(trans));

	trans.section_a.hdr.counter = UINT32_MAX - 1;
	trans.section_a.state = NVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.counter = UINT32_MAX - 2;
	trans.section_b.state = NVRAM_STATE_ALL_VERIFIED;
	trans.active = NVRAM_ACTIVE_A;

	struct nvram_header new;
	enum nvram_operation op = nvram_next_transaction(&trans, &new);
	if (op != (NVRAM_OPERATION_WRITE_B | NVRAM_OPERATION_COUNTER_RESET)) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.counter != 1) {
		printf("returned counter %u is not %u\n", new.counter, 1);
		goto error_exit;
	}

	trans.section_a.hdr.counter = UINT32_MAX - 2;
	trans.section_a.state = NVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.counter = UINT32_MAX - 1;
	trans.section_b.state = NVRAM_STATE_ALL_VERIFIED;
	trans.active = NVRAM_ACTIVE_B;

	op = nvram_next_transaction(&trans, &new);
	if (op != (NVRAM_OPERATION_WRITE_A | NVRAM_OPERATION_COUNTER_RESET)) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.counter != 1) {
		printf("returned counter %u is not %u\n", new.counter, 1);
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_nvram_update_transaction()
{
	struct nvram_header hdr1;
	hdr1.counter = 16;
	hdr1.data_len = 39;
	hdr1.data_crc32 = 0x12345678;
	hdr1.header_crc32 = 0x87654321;

	struct nvram_header hdr2;
	hdr2.counter = 10;
	hdr2.data_len = 10;
	hdr2.data_crc32 = 0xfedcba98;
	hdr2.header_crc32 = 0x89abcdef;

	struct nvram_header hdr3;
	hdr3.counter = 866;
	hdr3.data_len = 9;
	hdr3.data_crc32 = 0x00ff00ff;
	hdr3.header_crc32 = 0xff00ff00;

	struct nvram_transaction trans;
	memset(&trans, 0, sizeof(trans));
	memcpy(&trans.section_a.hdr, &hdr1, sizeof(hdr1));
	memcpy(&trans.section_b.hdr, &hdr2, sizeof(hdr2));
	trans.active = NVRAM_ACTIVE_A;
	nvram_update_transaction(&trans, NVRAM_OPERATION_WRITE_B, &hdr3);
	if (memcmp(&trans.section_a.hdr, &hdr1, sizeof(hdr1))) {
		printf("section_a hdr wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_b hdr wrong\n");
		goto error_exit;
	}
	if (trans.active != NVRAM_ACTIVE_B) {
		printf("wrong active\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	memcpy(&trans.section_a.hdr, &hdr2, sizeof(hdr2));
	memcpy(&trans.section_b.hdr, &hdr1, sizeof(hdr1));
	trans.active = NVRAM_ACTIVE_B;
	nvram_update_transaction(&trans, NVRAM_OPERATION_WRITE_A, &hdr3);
	if (memcmp(&trans.section_a.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_a hdr wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr1, sizeof(hdr1))) {
		printf("section_b hdr wrong\n");
		goto error_exit;
	}
	if (trans.active != NVRAM_ACTIVE_A) {
		printf("wrong active\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	memcpy(&trans.section_a.hdr, &hdr1, sizeof(hdr1));
	memcpy(&trans.section_b.hdr, &hdr2, sizeof(hdr2));
	trans.active = NVRAM_ACTIVE_A;
	nvram_update_transaction(&trans, NVRAM_OPERATION_WRITE_B | NVRAM_OPERATION_COUNTER_RESET, &hdr3);
	if (memcmp(&trans.section_a.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_a hdr wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_b hdr wrong\n");
		goto error_exit;
	}
	if (trans.active != (NVRAM_ACTIVE_A | NVRAM_ACTIVE_B)) {
		printf("wrong active\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

struct test test_array[] = {
		ADD_TEST(test_nvram_init_transaction),
		ADD_TEST(test_nvram_init_transaction_corrupt_header),
		ADD_TEST(test_nvram_init_transaction_corrupt_data),
		ADD_TEST(test_nvram_next_transaction),
		ADD_TEST(test_nvram_next_transaction_new),
		ADD_TEST(test_nvram_next_transaction_counter_reset),
		ADD_TEST(test_nvram_update_transaction),
		{NULL, NULL},
};

