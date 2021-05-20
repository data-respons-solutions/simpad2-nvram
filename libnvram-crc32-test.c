#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "crc32.h"

static int test_crc32_1(void)
{
	const uint8_t data[] = {
		0x05, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
		0x54, 0x45, 0x53, 0x54, 0x31, 0x61, 0x62, 0x63,
		0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x05,
		0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x54,
		0x45, 0x53, 0x54, 0x32, 0x64, 0x65, 0x66
	};
	const uint32_t data_crc32 = 0x6c9dd729;
	const uint32_t crc32 = calc_crc32(data, sizeof(data));

	if (data_crc32 != crc32) {
		printf("0x%08x != 0x%08x\n", data_crc32, crc32);
		return 1;
	}

	return 0;
}

static int test_crc32_2(void)
{
	const uint8_t data[] = {
		0x10, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
		0x29, 0xd7, 0x9d, 0x6c
	};
	const uint32_t data_crc32 = 0x0b4ad6e8;
	const uint32_t crc32 = calc_crc32(data, sizeof(data));

	if (data_crc32 != crc32) {
		printf("0x%08x != 0x%08x\n", data_crc32, crc32);
		return 1;
	}

	return 0;
}

static int test_crc32_empty(void)
{
	const uint8_t *data = NULL;
	const uint32_t data_crc32 = 0x0;
	const uint32_t crc32 = calc_crc32(data, 0);

	if (data_crc32 != crc32) {
		printf("0x%08x != 0x%08x\n", data_crc32, crc32);
		return 1;
	}

	return 0;
}

static int test_crc32_data_zero(void)
{
	const uint8_t data[] = {
		0x00, 0x00, 0x00, 0x00
	};
	const uint32_t data_crc32 = 0x38fb2284;
	const uint32_t crc32 = calc_crc32(data, sizeof(data));

	if (data_crc32 != crc32) {
		printf("0x%08x != 0x%08x\n", data_crc32, crc32);
		return 1;
	}

	return 0;
}

struct test {
	char* name;
	int (*func)(void);
};

#define ADD_TEST(NAME) {#NAME, &NAME}

struct test test_array[] = {
		ADD_TEST(test_crc32_1),
		ADD_TEST(test_crc32_2),
		ADD_TEST(test_crc32_empty),
		ADD_TEST(test_crc32_data_zero),
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
		printf("%s: %s\n", test_array[i].name, r ? "FAIL" : "PASS");
	}

	printf("Result: %s\n", errors ? "FAIL" : "PASS");

	return errors;
}
