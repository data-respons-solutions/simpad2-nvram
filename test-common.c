#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "libnvram.h"
#include "crc32.h"
#include "test-common.h"

// return 0 for equal
int keycmp(const uint8_t* key1, uint32_t key1_len, const uint8_t* key2, uint32_t key2_len)
{
	if (key1_len == key2_len) {
		return memcmp(key1, key2, key1_len);
	}
	return 1;
}

// return 0 for equal
int entrycmp(const struct libnvram_entry* entry1, const struct libnvram_entry* entry2)
{
	if (!keycmp(entry1->key, entry1->key_len, entry2->key, entry2->key_len)) {
		return keycmp(entry1->value, entry1->value_len, entry2->value, entry2->value_len);
	}
	return 1;
}

void fill_entry(struct libnvram_entry* entry, const char* key, const char* value)
{
	entry->key = (uint8_t*) key;
	entry->key_len = strlen(key);
	entry->value = (uint8_t*) value;
	entry->value_len = strlen(value);
}

struct libnvram_header make_header(uint32_t user, uint8_t type, uint32_t len, uint32_t crc32)
{
	struct libnvram_header hdr;
	hdr.magic = 0xb32c41b4;
	hdr.user = user;
	hdr.type = type;
	memset(hdr.reserved, 0, 3);
	hdr.len = len;
	hdr.crc32 = crc32;
	hdr.hdr_crc32 = calc_crc32((uint8_t*)&hdr, sizeof(hdr) - 4);
	return hdr;
}

uint32_t len;
uint32_t crc32;
uint32_t hdr_crc32;

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
