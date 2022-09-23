#include <inttypes.h>
#include "libnvram.h"

// return 0 for equal
int keycmp(const uint8_t* key1, uint32_t key1_len, const uint8_t* key2, uint32_t key2_len);

// return 0 for equal
int entrycmp(const struct libnvram_entry* entry1, const struct libnvram_entry* entry2);

void fill_entry(struct libnvram_entry* entry, const char* key, const char* value);
struct libnvram_header make_header(uint32_t user, uint8_t type, uint32_t len, uint32_t crc32);

// Testing boilerplate
struct test {
	char* name;
	int (*func)(void);
};
#define ADD_TEST(NAME) {#NAME, &NAME}
extern struct test test_array[];
