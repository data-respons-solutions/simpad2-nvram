/*
 * Copyright (C) 2019 Data Respons Solutions AB
 *
 * Author: Mikko Salomäki <ms@datarespons.se>
 *
 * SPDX-License-Identifier:	MIT
 */
#ifdef __UBOOT__
#include <common.h>
#include <stdlib.h>
#include <inttypes.h>
#include <linux/types.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#endif
#include "libnvram.h"
#include "crc32.h"

#ifdef DEBUG
#define LIBDEBUG 1
#else
#define LIBDEBUG 0
#endif

#define libdebug(fmt, ...) \
		do { if (LIBDEBUG) printf("DEBUG %s: " fmt, __func__, ##__VA_ARGS__); } while (0)

// Size of counter, len and crc32
#define NVRAM_HEADER_COUNTER_OFFSET 0
#define NVRAM_HEADER_SIZE_OFFSET 4
#define NVRAM_HEADER_CRC32_OFFSET 8
#define NVRAM_HEADER_DATA_OFFSET 12
#define NVRAM_HEADER_SIZE sizeof(uint32_t) * 3
#define NVRAM_MIN_SIZE NVRAM_HEADER_SIZE + NVRAM_ENTRY_MIN_SIZE

#define NVRAM_ENTRY_KEY_LEN_OFFSET 0
#define NVRAM_ENTRY_VALUE_LEN_OFFSET 4
#define NVRAM_ENTRY_DATA_OFFSET 8
// Size of key_len and value_len
#define NVRAM_ENTRY_HEADER_SIZE sizeof(uint32_t) * 2
//Minimum possible size of an entry where both key and value are 1 char each
#define NVRAM_ENTRY_MIN_SIZE NVRAM_ENTRY_HEADER_SIZE + 2

#define NVRAM_CRC32_INIT	0xffffffff
#define NVRAM_CRC32_XOR		0xffffffff

/*
 * Little endian to host
 *
 * @params
 * 	 le: array of length 4
 *
 * @returns
 *   host
 */
static uint32_t letou32(const uint8_t* le)
{
	return	  ((uint32_t) le[0] << 0)
			| ((uint32_t) le[1] << 8)
			| ((uint32_t) le[2] << 16)
			| ((uint32_t) le[3] << 24);
}

/*
 * Host to little endian
 *
 * @params
 * 	 host: host to convert
 *
 * @returns
 *   little endian
 */

static uint32_t u32tole(uint32_t host)
{
	uint8_t data[4] = {0,0,0,0};
	memcpy(&data, &host, sizeof(host));
	return	  ((uint32_t) data[0] << 0)
			| ((uint32_t) data[1] << 8)
			| ((uint32_t) data[2] << 16)
			| ((uint32_t) data[3] << 24);
}

/*
 * Create nvram entry from key and value.

 * @returns
 *   nvram_entry (Resource should be released with destroy_nvram_entry() by caller)
 *   NULL for failure.
 */
static struct nvram_entry*
create_nvram_entry(const uint8_t* key, uint32_t key_len, const uint8_t* value, uint32_t value_len)
{
	const size_t size = key_len + value_len + sizeof(struct nvram_entry);
	struct nvram_entry *entry = malloc(size);
	if (!entry) {
		libdebug("%s: malloc failed: %zu bytes\n", __func__, size);
		return NULL;
	}
	entry->key = (uint8_t*) entry + sizeof(struct nvram_entry);
	memcpy(entry->key, key, key_len);
	entry->key_len = key_len;

	entry->value = (uint8_t*) entry + sizeof(struct nvram_entry) + key_len;
	memcpy(entry->value, value, value_len);
	entry->value_len = value_len;

	return entry;
}

/*
 * Verify buffer contains valid nvram entry
 *
 * @params
 *   data: data buffer
 *   max_len: LengtH of buffer
 *   key_len: Returns length of key
 *   value_len: Returns length of value
 *
 * @returns
 *   0 for invalid, 1 for valid
 */
static int is_valid_nvram_entry(const uint8_t* data, uint32_t max_len, uint32_t* key_len, uint32_t* value_len)
{
	if (!data) {
		libdebug("empty data\n");
		return 0;
	}

	if (max_len < NVRAM_ENTRY_MIN_SIZE) {
		libdebug("data too small to fit one entry\n");
		return 0;
	}

	const uint32_t _key_len = letou32(data);
	const uint32_t _value_len = letou32(data + 4);
	const uint32_t required_len = _key_len + _value_len + NVRAM_ENTRY_HEADER_SIZE;
	if (required_len > max_len) {
		libdebug("defined length [%" PRIu32 "] longer than buffer [%" PRIu32 "]\n",
				required_len, max_len);
		return 0;
	}

	*key_len = _key_len;
	*value_len = _value_len;

	return 1;
}

/* Fill nvram entry to buffer and return size used */
static uint32_t fill_nvram_entry(uint8_t* data, const struct nvram_entry* entry)
{
	const uint32_t le_key_len = u32tole(entry->key_len);
	const uint32_t le_value_len = u32tole(entry->value_len);
	memcpy(data + NVRAM_ENTRY_KEY_LEN_OFFSET, &le_key_len, sizeof(le_key_len));
	memcpy(data + NVRAM_ENTRY_VALUE_LEN_OFFSET, &le_value_len, sizeof(le_value_len));
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET, entry->key, entry->key_len);
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET + entry->key_len, entry->value, entry->value_len);

	const uint32_t entry_size = entry->key_len + entry->value_len + NVRAM_ENTRY_HEADER_SIZE;
	return entry_size;
}

/* Calculate size needed for all entries in linked node */
static uint32_t calc_serialized_entries_size(const struct nvram_list* list)
{
	uint32_t size = 0;
	struct nvram_list *cur = (struct nvram_list*) list;
	while(cur) {
		size += NVRAM_ENTRY_HEADER_SIZE + cur->entry->key_len + cur->entry->value_len;
		cur = cur->next;
	}
	return size;
}

/* Fill nvram header */
static void fill_nvram_header(uint8_t* data, uint32_t counter, uint32_t data_size)
{
	const uint32_t le_counter = u32tole(counter);
	const uint32_t le_data_size = u32tole(data_size);
	uint32_t le_crc32 = 0;

	memcpy(data + NVRAM_HEADER_COUNTER_OFFSET, &le_counter, sizeof(le_counter));
	memcpy(data + NVRAM_HEADER_SIZE_OFFSET, &le_data_size, sizeof(le_data_size));
	memcpy(data + NVRAM_HEADER_CRC32_OFFSET, &le_crc32, sizeof(le_crc32));
	le_crc32 = u32tole(calc_crc32(NVRAM_CRC32_INIT, data, data_size + NVRAM_HEADER_SIZE) ^ NVRAM_CRC32_XOR);
	memcpy(data + NVRAM_HEADER_CRC32_OFFSET, &le_crc32, sizeof(le_crc32));
}

static void destroy_nvram_entry(struct nvram_entry* entry)
{
	if (entry) {
		free(entry);
		entry = NULL;
	}
}

void destroy_nvram_list(struct nvram_list** list)
{
	struct nvram_list *cur = *list;
	while (cur) {
		destroy_nvram_entry(cur->entry);
		struct nvram_list *prev = cur;
		cur = cur->next;
		free(prev);
	}
	*list = NULL;
}

// return 0 for equal
static int keycmp(const uint8_t* key1, uint32_t key1_len, const uint8_t* key2, uint32_t key2_len)
{
	if (key1_len == key2_len) {
		return memcmp(key1, key2, key1_len);
	}
	return 1;
}

int nvram_list_set(struct nvram_list** list, const struct nvram_entry* entry)
{
	struct nvram_list* prev = NULL;
	struct nvram_list* cur = *list;
	while (cur) {
		if (!keycmp(cur->entry->key, cur->entry->key_len, entry->key, entry->key_len)) {
			if (keycmp(cur->entry->value, cur->entry->value_len, entry->value, entry->value_len)) {
				// replace entry
				break;
			}
			// already exists
			return 0;
		}
		prev = cur;
		cur = cur->next;
	}

	struct nvram_entry *new = create_nvram_entry(entry->key, entry->key_len, entry->value, entry->value_len);
	if (!new) {
		return -ENOMEM;
	}

	if (!cur) {
		// new entry
		cur = malloc(sizeof(struct nvram_list));
		if (!cur) {
			destroy_nvram_entry(new);
			return -ENOMEM;
		}
		cur->entry = new;
		cur->next = NULL;
		if (prev) {
			prev->next = cur;
		}
		if (!*list) {
			*list = cur;
		}
	}
	else {
		// replace entry
		destroy_nvram_entry(cur->entry);
		cur->entry = new;
	}

	return 0;
}

struct nvram_entry* nvram_list_get(const struct nvram_list* list, const uint8_t* key, uint32_t key_len)
{
	struct nvram_list* cur = (struct nvram_list*) list;
	while(cur) {
		if (!keycmp(cur->entry->key, cur->entry->key_len, key, key_len)) {
			return cur->entry;
		}
		cur = cur->next;
	}

	return NULL;
}

int nvram_list_remove(struct nvram_list** list, const uint8_t* key, uint32_t key_len)
{
	struct nvram_list* prev = NULL;
	struct nvram_list* cur = *list;
	while(cur) {
		if (!keycmp(cur->entry->key, cur->entry->key_len, key, key_len)) {
			destroy_nvram_entry(cur->entry);
			if (prev) {
				prev->next = cur->next;
			}
			else {
				*list = cur->next;
			}
			free(cur);
			return 1;
		}
		prev = cur;
		cur = cur->next;
	}

	return 0;
}

int is_valid_nvram_section(const uint8_t* data, uint32_t len, uint32_t* data_len, uint32_t* counter)
{
	if (len < NVRAM_HEADER_SIZE) {
		libdebug("data too small\n");
		return 0;
	}

	const uint32_t _counter = letou32(data);
	const uint32_t _data_len = letou32(data + 4);
	const uint32_t _data_crc32 = letou32(data + 8);

	if (UINT32_MAX - NVRAM_HEADER_SIZE < _data_len)  {
		libdebug("header defined data[%" PRIu32 "] too large\n", _data_len);
		return 0;
	}

	if (len < NVRAM_HEADER_SIZE + _data_len) {
		libdebug("header defines length[%" PRIu32 "] longer than buffer[%" PRIu32 "]\n", (uint32_t) NVRAM_HEADER_SIZE + _data_len, len);
		return 0;
	}

	const uint32_t zero = 0;
	uint32_t crc32 = calc_crc32(NVRAM_CRC32_INIT, data, NVRAM_HEADER_CRC32_OFFSET);
	crc32 = calc_crc32(crc32,(uint8_t*) &zero, sizeof(zero));
	crc32 = calc_crc32(crc32, data + NVRAM_HEADER_SIZE, _data_len);
	crc32 ^= NVRAM_CRC32_XOR;

	if (crc32 != _data_crc32) {
		libdebug("crc32 mismatch: 0x%" PRIx32" != 0x%" PRIx32 "\n", crc32, _data_crc32);
		return 0;
	}

	*data_len = _data_len + NVRAM_HEADER_SIZE;
	*counter = _counter;

	return 1;
}

int nvram_section_deserialize(struct nvram_list** list, const uint8_t* data, uint32_t len)
{
	if (!data) {
		libdebug("data empty\n");
		return -EINVAL;
	}
	if (*list) {
		libdebug("list not empty\n");
		return -EINVAL;
	}

	for (uint32_t i = NVRAM_HEADER_SIZE; i < len;) {
		uint32_t remaining = (i > len) ? i - len : len - i;
		uint32_t key_len = 0;
		uint32_t value_len = 0;
		if (!is_valid_nvram_entry(data + i, remaining, &key_len, &value_len)) {
			return -EINVAL;
		}

		struct nvram_entry entry;
		entry.key = (uint8_t*) data + i + NVRAM_ENTRY_HEADER_SIZE;
		entry.key_len = key_len;
		entry.value = (uint8_t*) data + i + NVRAM_ENTRY_HEADER_SIZE + key_len;
		entry.value_len = value_len;

		int r = nvram_list_set(list, &entry);
		if (r) {
			libdebug("Failed creating entry\n");
			return r;
		}

		i += NVRAM_ENTRY_HEADER_SIZE + key_len + value_len;
	}

	return 0;
}

int nvram_section_serialize_size(const struct nvram_list* list, uint32_t* size)
{
	if (!list) {
		libdebug("list empty\n");
		return -EINVAL;
	}

	const uint32_t data_len = calc_serialized_entries_size(list);
	const uint32_t buf_len = NVRAM_HEADER_SIZE + data_len;

	*size = buf_len;

	return 0;
}

int nvram_section_serialize(const struct nvram_list* list, uint32_t counter, uint8_t* data, uint32_t size)
{
	if (!list) {
		libdebug("list empty\n");
		return -EINVAL;
	}

	const uint32_t data_len = calc_serialized_entries_size(list);
	const uint32_t buf_len = NVRAM_HEADER_SIZE + data_len;
	if (size < buf_len) {
		libdebug("data buffer too small\n");
		return -EINVAL;
	}

	struct nvram_list* cur = (struct nvram_list*) list;
	for (uint32_t i = NVRAM_HEADER_SIZE; cur; cur = cur->next) {
		i += fill_nvram_entry(data + i, cur->entry);
	}

	fill_nvram_header(data, counter, data_len);

	return 0;
}
