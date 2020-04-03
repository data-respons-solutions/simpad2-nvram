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
 * Create nvram node from data buffer containing key (of key_len) and value (of value_len).
 *  Expects key at data[0] and value at data[key_len].
 *  Neither key nor value should be null terminated.
 *
 * @params
 *   data: data buffer
 *   key_len: Length of key
 *   value_len: Length of value
 *
 * @returns
 *   nvram_node (Resource should be released with destroy_nvram_node() by caller)
 *
 *   NULL for failure.
 */
static struct nvram_node*
create_nvram_node(const uint8_t* key, uint32_t key_len, const uint8_t* value, uint32_t value_len)
{
	const uint32_t key_str_len = key_len + 1;
	const uint32_t value_str_len = value_len + 1;
	const size_t sz = key_str_len + value_str_len + sizeof(struct nvram_node);
	struct nvram_node* node = malloc(sz);
	if (!node) {
		libdebug("malloc failed\n");
		return NULL;
	}

	node->key = (char*) node + sizeof(struct nvram_node);
	memcpy(node->key, (char*) key, key_len);
	memset(node->key + key_len, '\0', 1);

	node->value = (char*) node + sizeof(struct nvram_node) + key_str_len;
	memcpy(node->value, (char*) value, value_len);
	memset(node->value + value_len, '\0', 1);
	node->next = NULL;

	libdebug("new: %s = %s\n", node->key, node->value);

	return node;
}

static struct nvram_node*
create_nvram_node_str(const char* key, const char* value)
{
	return create_nvram_node((uint8_t*) key, strlen(key), (uint8_t*) value, strlen(value));
}

static struct nvram_node*
create_nvram_node_raw(const uint8_t* data, uint32_t key_len, uint32_t value_len)
{
	return create_nvram_node(data, key_len, data + key_len, value_len);
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
static uint32_t fill_nvram_entry(uint8_t* data, const char* key, const char* value)
{
	const uint32_t key_len = strlen(key);
	const uint32_t le_key_len = u32tole(key_len);
	const uint32_t value_len = strlen(value);
	const uint32_t le_value_len = u32tole(value_len);
	memcpy(data + NVRAM_ENTRY_KEY_LEN_OFFSET, &le_key_len, sizeof(le_key_len));
	memcpy(data + NVRAM_ENTRY_VALUE_LEN_OFFSET, &le_value_len, sizeof(le_value_len));
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET, key, key_len);
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET + key_len, value, value_len);

	const uint32_t entry_size = key_len + value_len + NVRAM_ENTRY_HEADER_SIZE;
	return entry_size;
}

/* Calculate size needed for all entries in linked node */
static uint32_t calc_serialized_entries_size(const struct nvram_node* node)
{
	uint32_t size = 0;
	while(node) {
		size += NVRAM_ENTRY_HEADER_SIZE + strlen(node->key) + strlen(node->value);
		node = node->next;
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

static void destroy_nvram_node(struct nvram_node* node)
{
	if (node) {
		free(node);
		node = NULL;
	}
}

void destroy_nvram_list(struct nvram_list* list)
{
	if (list->entry) {
		struct nvram_node* cur = list->entry;
		struct nvram_node* next = NULL;
		while (cur) {
			next = cur->next;
			destroy_nvram_node(cur);
			cur = next;
		}
		list->entry = NULL;
	}
}

int nvram_list_set(struct nvram_list* list, const char* key, const char* value)
{
	struct nvram_node* next = NULL;
	struct nvram_node* prev = NULL;
	struct nvram_node* cur = NULL;
	for (cur = list->entry; cur; cur = next) {
		next = cur->next;
		if (!strcmp(key, cur->key)) {
			if (!strcmp(value, cur->value)) {
				return 0;
			}
			// replace node
			break;
		}
		prev = cur;
	}

	struct nvram_node* new = create_nvram_node_str(key, value);
	if (!new) {
		return -ENOMEM;
	}
	new->next = next;

	if (!prev) {
		// create or replace first node
		if (list->entry) {
			destroy_nvram_node(list->entry);
		}
		list->entry = new;
	}
	else {
		destroy_nvram_node(cur);
		prev->next = new;
	}

	return 0;
}

char* nvram_list_get(const struct nvram_list* list, const char* key)
{
	struct nvram_node* cur = list->entry;
	while(cur) {
		if (!strcmp(key, cur->key)) {
			return cur->value;
		}
		cur = cur->next;
	}

	return NULL;
}

int nvram_list_remove(struct nvram_list* list, const char* key)
{
	struct nvram_node* next = NULL;
	struct nvram_node* prev = NULL;
	struct nvram_node* cur = NULL;
	for (cur = list->entry; cur; cur = next) {
		next = cur->next;
		if (!strcmp(key, cur->key)) {
			// remove node
			if (!prev) {
				// remove first node
				destroy_nvram_node(list->entry);
				list->entry = next;
			}
			else{
				destroy_nvram_node(cur);
				prev->next = next;
			}
			return 1;

		}
		prev = cur;
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

int nvram_section_deserialize(struct nvram_list* list, const uint8_t* data, uint32_t len)
{
	if (!data) {
		libdebug("data empty\n");
		return -EINVAL;
	}
	if (list->entry) {
		libdebug("list not empty\n");
		return -EINVAL;
	}

	struct nvram_node* new = NULL;
	struct nvram_node* cur = NULL;
	for (uint32_t i = NVRAM_HEADER_SIZE; i < len;) {
		uint32_t remaining = (i > len) ? i - len : len - i;
		uint32_t key_len = 0;
		uint32_t value_len = 0;
		if (!is_valid_nvram_entry(data + i, remaining, &key_len, &value_len)) {
			return -EINVAL;
		}

		if(cur) {
			cur->next = create_nvram_node_raw(data + i + NVRAM_ENTRY_HEADER_SIZE, key_len, value_len);
			cur = cur->next;
		}
		else {
			cur = create_nvram_node_raw(data + i + NVRAM_ENTRY_HEADER_SIZE, key_len, value_len);
		}

		if (!cur) {
			libdebug("Failed creating node\n");
			return -ENOMEM;
		}

		if (!new) {
			new = cur;
		}
		i += NVRAM_ENTRY_HEADER_SIZE + key_len + value_len;
	}

	list->entry = new;

	return 0;
}

int nvram_section_serialize_size(const struct nvram_list* list, uint32_t* size)
{
	if (!list) {
		libdebug("list empty\n");
		return -EINVAL;
	}

	const uint32_t data_len = calc_serialized_entries_size(list->entry);
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

	const uint32_t data_len = calc_serialized_entries_size(list->entry);
	const uint32_t buf_len = NVRAM_HEADER_SIZE + data_len;
	if (size < buf_len) {
		libdebug("data buffer too small\n");
		return -EINVAL;
	}

	struct nvram_node* cur = list->entry;
	for (uint32_t i = NVRAM_HEADER_SIZE; cur; cur = cur->next) {
		i += fill_nvram_entry(data + i, cur->key, cur->value);
	}

	fill_nvram_header(data, counter, data_len);

	return 0;
}
