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

int is_valid_nvram_entry(const uint8_t* data, uint32_t max_len, uint32_t* key_len, uint32_t* value_len)
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

int nvram_serialize (struct nvram_node* const node, uint8_t** data, uint32_t* len)
{
	if (!node) {
		debug("node empty\n");
		return -EINVAL;
	}

	if (*data) {
		debug("data not empty\n");
		return -EINVAL;
	}

	// get needed buf size
	uint32_t sz = 0;
	struct nvram_node* cur = node;
	while (cur) {
		sz += NVRAM_ENTRY_HEADER_SIZE + strlen(cur->key) + strlen(cur->value);
		cur = cur->next;
	}
	debug("allocating buffer: %" PRIu32 "b\n", sz);
	uint8_t* buf = malloc(sz);
	if (!buf) {
		debug("Malloc failed\n");
		return -ENOMEM;
	}

	cur = node;
	for (uint32_t i = 0; cur; cur = cur->next) {
		uint32_t key_len = u32tole(strlen(cur->key));
		uint32_t value_len = u32tole(strlen(cur->value));
		memcpy(buf + i, &key_len, sizeof(key_len));
		memcpy(buf + i + sizeof(key_len), &value_len, sizeof(value_len));
		memcpy(buf + i + sizeof(key_len) + sizeof(value_len), cur->key, key_len);
		memcpy(buf + i + sizeof(key_len) + sizeof(value_len) + key_len, cur->value, value_len);
		i += sizeof(key_len) + sizeof(value_len) + key_len + value_len;
	}

	*data = buf;
	*len = sz;

	return 0;
}

int nvram_deserialize(struct nvram_node** node, const uint8_t* buf, uint32_t len)
{
	if (*node) {
		debug("node not empty\n");
		return -EINVAL;
	}

	struct nvram_node* new = NULL;
	struct nvram_node* cur = NULL;

	for (uint32_t i = 0; i < len;) {
		uint32_t remaining = (i > len) ? i - len : len - i;
		uint32_t key_len = 0;
		uint32_t value_len = 0;
		if (!is_valid_nvram_entry(buf + i, remaining, &key_len, &value_len)) {
			return -EINVAL;
		}

		if(cur) {
			cur->next = create_nvram_node(buf + i + NVRAM_ENTRY_HEADER_SIZE, key_len, value_len);
			cur = cur->next;
		}
		else {
			cur = create_nvram_node(buf + i + NVRAM_ENTRY_HEADER_SIZE, key_len, value_len);
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

int is_valid_nvram_section(const uint8_t* data, uint32_t len, uint32_t* data_len)
{
	if (len < NVRAM_HEADER_SIZE) {
		debug("data too small\n");
		return 0;
	}

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

	return 1;
}
