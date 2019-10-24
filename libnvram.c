#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include "libnvram.h"
#include "crc32.h"


#define DEBUG 1

#define debug(fmt, ...) \
		do { if (DEBUG) printf("DEBUG %s: " fmt, __func__, ##__VA_ARGS__); } while (0)

#define error(fmt, ...) \
		do { if (DEBUG) printf("ERROR %s: " fmt, __func__, ##__VA_ARGS__); } while (0)

// Size of counter, len and crc32
#define NVRAM_HEADER_COUNTER_OFFSET 0
#define NVRAM_HEADER_SIZE_OFFSET 4
#define NVRAM_HEADER_CRC32_OFFSET 8
#define NVRAM_HEADER_DATA_OFFSET 12
#define NVRAM_HEADER_SIZE sizeof(uint32_t) * 3
#define NVRAM_MIN_SIZE NVRAM_HEADER_SIZE + NVRAM_ENTRY_MIN_SIZE

/*
 * Serialized nvram entry is stored as key/value entries in format:
 *
 * key_len|value_len|key|value
 *
 * uint32_t key_len = Length of key string
 * uint32_t value_len = Lenght of value string
 * uint8_t[] key = Key string NOT including null terminator
 * uint8_t[] value = Value string NOT including null terminator
 */
#define NVRAM_ENTRY_KEY_LEN_OFFSET 0
#define NVRAM_ENTRY_VALUE_LEN_OFFSET 4
#define NVRAM_ENTRY_DATA_OFFSET 8
// Size of key_len and value_len
#define NVRAM_ENTRY_HEADER_SIZE sizeof(uint32_t) * 2
//Minimum possible size of an entry where both key and value are 1 char each
#define NVRAM_ENTRY_MIN_SIZE NVRAM_ENTRY_HEADER_SIZE + 2

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
create_nvram_node(const uint8_t* data, uint32_t key_len, uint32_t value_len)
{
	const uint32_t key_str_len = key_len + 1;
	const uint32_t value_str_len = value_len + 1;
	size_t sz = key_str_len + value_str_len + sizeof(struct nvram_node);
	struct nvram_node* node = malloc(sz);
	if (!node) {
		debug("malloc failed\n");
		return NULL;
	}

	node->key = (char*) node + sizeof(struct nvram_node);
	memcpy(node->key, (char*) data, key_len);
	node->key[key_str_len] = '\0';
	node->value = (char*) node + sizeof(struct nvram_node) + key_str_len;
	memcpy(node->value, (char*) data + key_len, value_len);
	node->value[value_str_len] = '\0';
	node->next = NULL;

	debug("new: %s = %s\n", node->key, node->value);

	return node;
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
		debug("empty data\n");
		return 0;
	}

	if (max_len < NVRAM_ENTRY_MIN_SIZE) {
		debug("data too small to fit one entry\n");
		return 0;
	}

	const uint32_t _key_len = letou32(data);
	const uint32_t _value_len = letou32(data + 4);
	const uint32_t required_len = _key_len + _value_len + NVRAM_ENTRY_HEADER_SIZE;
	if (required_len > max_len) {
		debug("defined length [%" PRIu32 "] longer than buffer [%" PRIu32 "]\n",
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

	return key_len + value_len + NVRAM_ENTRY_HEADER_SIZE;
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
	const uint32_t le_crc32 = u32tole(calc_crc32(0, data + NVRAM_HEADER_SIZE, data_size));
	memcpy(data + NVRAM_HEADER_COUNTER_OFFSET, &le_counter, sizeof(le_counter));
	memcpy(data + NVRAM_HEADER_SIZE_OFFSET, &le_data_size, sizeof(le_data_size));
	memcpy(data + NVRAM_HEADER_CRC32_OFFSET, &le_crc32, sizeof(le_crc32));
}

void destroy_nvram_node(struct nvram_node* node)
{
	if (node) {
		if (node->next) {
			destroy_nvram_node(node->next);
		}
		free(node);
		node = NULL;
	}
}

int is_valid_nvram_section(const uint8_t* data, uint32_t len, uint32_t* data_len, uint32_t* counter)
{
	if (len < NVRAM_HEADER_SIZE) {
		debug("data too small\n");
		return 0;
	}

	const uint32_t _counter = letou32(data);
	const uint32_t _data_len = letou32(data + 4);
	const uint32_t _data_crc32 = letou32(data + 8);

	if (len < NVRAM_HEADER_SIZE + _data_len) {
		debug("header defines length[%" PRIu32 "] longer than buffer[%" PRIu32 "]\n", (uint32_t) NVRAM_HEADER_SIZE + _data_len, len);
		return 0;
	}

	uint32_t crc32 = calc_crc32(0, data + NVRAM_HEADER_SIZE, _data_len);
	if (crc32 != _data_crc32) {
		debug("crc32 mismatch: 0x%" PRIx32" != 0x%" PRIx32 "\n", crc32, _data_crc32);
		return 0;
	}

	*data_len = _data_len;
	*counter = _counter;

	return 1;
}

int nvram_section_deserialize(struct nvram_node** node, const uint8_t* data, uint32_t len)
{
	if (!data) {
		debug("data empty\n");
		return -EINVAL;
	}
	if (len < NVRAM_MIN_SIZE) {
		//debug("len(%" PRIu32 ") smaller than NVRAM_MIN_SIZE(%" PRIu32 ")\n", len, NVRAM_MIN_SIZE);
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
			cur->next = create_nvram_node(data + i + NVRAM_ENTRY_HEADER_SIZE, key_len, value_len);
			cur = cur->next;
		}
		else {
			cur = create_nvram_node(data + i + NVRAM_ENTRY_HEADER_SIZE, key_len, value_len);
		}

		if (!cur) {
			debug("Failed creating node\n");
			return -ENOMEM;
		}

		if (!new) {
			new = cur;
		}
		i += NVRAM_ENTRY_HEADER_SIZE + key_len + value_len;
	}

	*node = new;

	return 0;
}

int nvram_section_serialize(struct nvram_node* const node, uint32_t counter, uint8_t** data, uint32_t* len)
{
	if (!node) {
		debug("node empty\n");
		return -EINVAL;
	}

	const uint32_t data_len = calc_serialized_entries_size(node);
	const uint32_t buf_len = NVRAM_HEADER_SIZE + data_len;
	debug("allocating: %" PRIu32 "b\n", buf_len);
	uint8_t* buf = malloc(buf_len);
	if (!buf) {
		debug("malloc failed\n");
		return -ENOMEM;
	}

	struct nvram_node* cur = node;
	for (uint32_t i = NVRAM_HEADER_SIZE; cur; cur = cur->next) {
		i += fill_nvram_entry(buf + i, cur->key, cur->value);
	}

	fill_nvram_header(buf, counter, data_len);

	*data = buf;
	*len = buf_len;

	return 0;
}
