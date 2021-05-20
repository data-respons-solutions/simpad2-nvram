#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "libnvram.h"
#include "test-common.h"

static int test_libnvram_init_transaction()
{
	const struct libnvram_header hdr1 = make_header(16, LIBNVRAM_TYPE_LIST, 39, 0x6c9dd729);
	const uint8_t data1[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x9b, 0xa2, 0x0a, 0x25,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	const struct libnvram_header hdr2 = make_header(10, LIBNVRAM_TYPE_LIST, 39, 0x6c9dd729);
	const uint8_t data2[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x0a, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x46, 0xf0, 0xe0, 0xf1,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct libnvram_transaction trans;

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, data1, sizeof(data1), data2, sizeof(data2));
	if (trans.active != LIBNVRAM_ACTIVE_A) {
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
	if (trans.section_a.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, data2, sizeof(data2), data1, sizeof(data1));
	if (trans.active != LIBNVRAM_ACTIVE_B) {
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
	if (trans.section_a.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, data1, sizeof(data1), data1, sizeof(data1));
	if (trans.active != (LIBNVRAM_ACTIVE_A | LIBNVRAM_ACTIVE_B)) {
		printf("Wrong section active\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_libnvram_init_transaction_corrupt_header()
{
	const struct libnvram_header hdr = make_header(16, LIBNVRAM_TYPE_LIST, 39, 0x6c9dd729);
	const uint8_t data1[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x9b, 0xa2, 0x0a, 0x25,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	const uint8_t data2[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x0a, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x50, 0xf0, 0xe0, 0xf1,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	struct libnvram_transaction trans;

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, data2, sizeof(data2), data1, sizeof(data1));
	if (trans.active != LIBNVRAM_ACTIVE_B) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr, sizeof(hdr))) {
		printf("header2 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != LIBNVRAM_STATE_HEADER_CORRUPT) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, data1, sizeof(data1), data2, sizeof(data2));
	if (trans.active != LIBNVRAM_ACTIVE_A) {
		printf("Wrong section active\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_a.hdr, &hdr, sizeof(hdr))) {
		printf("header1 wrong\n");
		goto error_exit;
	}
	if (trans.section_a.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != LIBNVRAM_STATE_HEADER_CORRUPT) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}
static int test_libnvram_init_transaction_corrupt_data()
{
	const struct libnvram_header hdr1 = make_header(16, LIBNVRAM_TYPE_LIST, 39, 0x6c9dd729);
	const uint8_t data1[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x10, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x9b, 0xa2, 0x0a, 0x25,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};

	const struct libnvram_header hdr2 = make_header(10, LIBNVRAM_TYPE_LIST, 39, 0x6c9dd729);
	const uint8_t corrupt[] = {
		0xb4, 0x41, 0x2c, 0xb3, 0x0a, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c, 0x46, 0xf0, 0xe0, 0xf1,
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x70
	};

	struct libnvram_transaction trans;

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, data1, sizeof(data1), corrupt, sizeof(corrupt));
	if (trans.active != LIBNVRAM_ACTIVE_A) {
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
	if (trans.section_a.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != (LIBNVRAM_STATE_HEADER_VERIFIED | LIBNVRAM_STATE_DATA_CORRUPT)) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	libnvram_init_transaction(&trans, corrupt, sizeof(corrupt), data1, sizeof(data1));
	if (trans.active != LIBNVRAM_ACTIVE_B) {
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
	if (trans.section_a.state != (LIBNVRAM_STATE_HEADER_VERIFIED | LIBNVRAM_STATE_DATA_CORRUPT)) {
		printf("section_a wrong state\n");
		goto error_exit;
	}
	if (trans.section_b.state != LIBNVRAM_STATE_ALL_VERIFIED) {
		printf("section_b wrong state\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_libnvram_next_transaction()
{
	struct libnvram_transaction trans;
	memset(&trans, 0, sizeof(trans));

	trans.section_a.hdr.user = 16;
	trans.section_a.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.user = 10;
	trans.section_b.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.active = LIBNVRAM_ACTIVE_A;

	struct libnvram_header new;
	enum libnvram_operation op = libnvram_next_transaction(&trans, &new);
	if (op != LIBNVRAM_OPERATION_WRITE_B) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.user != 17) {
		printf("returned counter %u is not %u\n", new.user, 17);
		goto error_exit;
	}

	trans.section_a.hdr.user = 10;
	trans.section_a.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.user = 6;
	trans.section_b.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.active = LIBNVRAM_ACTIVE_B;

	op = libnvram_next_transaction(&trans, &new);
	if (op != LIBNVRAM_OPERATION_WRITE_A) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.user != 7) {
		printf("returned counter %u is not %u\n", new.user, 7);
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_libnvram_next_transaction_new()
{
	struct libnvram_transaction trans;
	memset(&trans, 0, sizeof(trans));

	trans.section_a.state = LIBNVRAM_STATE_HEADER_CORRUPT;
	trans.section_b.state = LIBNVRAM_STATE_HEADER_CORRUPT;
	trans.active = LIBNVRAM_ACTIVE_NONE;

	struct libnvram_header new;
	enum libnvram_operation op = libnvram_next_transaction(&trans, &new);
	if (op != LIBNVRAM_OPERATION_WRITE_A) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.user != 1) {
		printf("returned counter %u is not %u\n", new.user, 1);
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_libnvram_next_transaction_counter_reset()
{
	struct libnvram_transaction trans;
	memset(&trans, 0, sizeof(trans));

	trans.section_a.hdr.user = UINT32_MAX - 1;
	trans.section_a.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.user = UINT32_MAX - 2;
	trans.section_b.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.active = LIBNVRAM_ACTIVE_A;

	struct libnvram_header new;
	enum libnvram_operation op = libnvram_next_transaction(&trans, &new);
	if (op != (LIBNVRAM_OPERATION_WRITE_B | LIBNVRAM_OPERATION_COUNTER_RESET)) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.user != 1) {
		printf("returned counter %u is not %u\n", new.user, 1);
		goto error_exit;
	}

	trans.section_a.hdr.user = UINT32_MAX - 2;
	trans.section_a.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.section_b.hdr.user = UINT32_MAX - 1;
	trans.section_b.state = LIBNVRAM_STATE_ALL_VERIFIED;
	trans.active = LIBNVRAM_ACTIVE_B;

	op = libnvram_next_transaction(&trans, &new);
	if (op != (LIBNVRAM_OPERATION_WRITE_A | LIBNVRAM_OPERATION_COUNTER_RESET)) {
		printf("wrong operation returned");
		goto error_exit;
	}
	if (new.user != 1) {
		printf("returned counter %u is not %u\n", new.user, 1);
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

static int test_libnvram_update_transaction()
{
	const struct libnvram_header hdr1 = make_header(16, LIBNVRAM_TYPE_LIST, 39, 0x12345678);
	const struct libnvram_header hdr2 = make_header(10, LIBNVRAM_TYPE_LIST, 10, 0xfedcba98);
	const struct libnvram_header hdr3 = make_header(866, LIBNVRAM_TYPE_LIST, 9, 0x00ff00ff);

	struct libnvram_transaction trans;
	memset(&trans, 0, sizeof(trans));
	memcpy(&trans.section_a.hdr, &hdr1, sizeof(hdr1));
	memcpy(&trans.section_b.hdr, &hdr2, sizeof(hdr2));
	trans.active = LIBNVRAM_ACTIVE_A;
	libnvram_update_transaction(&trans, LIBNVRAM_OPERATION_WRITE_B, &hdr3);
	if (memcmp(&trans.section_a.hdr, &hdr1, sizeof(hdr1))) {
		printf("section_a hdr wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_b hdr wrong\n");
		goto error_exit;
	}
	if (trans.active != LIBNVRAM_ACTIVE_B) {
		printf("wrong active\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	memcpy(&trans.section_a.hdr, &hdr2, sizeof(hdr2));
	memcpy(&trans.section_b.hdr, &hdr1, sizeof(hdr1));
	trans.active = LIBNVRAM_ACTIVE_B;
	libnvram_update_transaction(&trans, LIBNVRAM_OPERATION_WRITE_A, &hdr3);
	if (memcmp(&trans.section_a.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_a hdr wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr1, sizeof(hdr1))) {
		printf("section_b hdr wrong\n");
		goto error_exit;
	}
	if (trans.active != LIBNVRAM_ACTIVE_A) {
		printf("wrong active\n");
		goto error_exit;
	}

	memset(&trans, 0, sizeof(trans));
	memcpy(&trans.section_a.hdr, &hdr1, sizeof(hdr1));
	memcpy(&trans.section_b.hdr, &hdr2, sizeof(hdr2));
	trans.active = LIBNVRAM_ACTIVE_A;
	libnvram_update_transaction(&trans, LIBNVRAM_OPERATION_WRITE_B | LIBNVRAM_OPERATION_COUNTER_RESET, &hdr3);
	if (memcmp(&trans.section_a.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_a hdr wrong\n");
		goto error_exit;
	}
	if (memcmp(&trans.section_b.hdr, &hdr3, sizeof(hdr3))) {
		printf("section_b hdr wrong\n");
		goto error_exit;
	}
	if (trans.active != (LIBNVRAM_ACTIVE_A | LIBNVRAM_ACTIVE_B)) {
		printf("wrong active\n");
		goto error_exit;
	}

	return 0;
error_exit:
	return 1;
}

struct test test_array[] = {
		ADD_TEST(test_libnvram_init_transaction),
		ADD_TEST(test_libnvram_init_transaction_corrupt_header),
		ADD_TEST(test_libnvram_init_transaction_corrupt_data),
		ADD_TEST(test_libnvram_next_transaction),
		ADD_TEST(test_libnvram_next_transaction_new),
		ADD_TEST(test_libnvram_next_transaction_counter_reset),
		ADD_TEST(test_libnvram_update_transaction),
		{NULL, NULL},
};

