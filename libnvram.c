/*
 * Copyright (C) 2019 Data Respons Solutions AB
 *
 * Author: Mikko Salomäki <ms@datarespons.se>
 *
 * SPDX-License-Identifier:	MIT
 */
#ifdef __UBOOT__
#include <common.h>
#include <linux/types.h>
#else
#include <string.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include "libnvram.h"
#include "crc32.h"

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

static void destroy_nvram_entry(struct nvram_entry* entry)
{
	if (entry) {
		free(entry);
		entry = NULL;
	}
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
		return -NVRAM_ERROR_NOMEM;
	}

	if (!cur) {
		// new entry
		cur = malloc(sizeof(struct nvram_list));
		if (!cur) {
			destroy_nvram_entry(new);
			return -NVRAM_ERROR_NOMEM;
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

// le length must be 4
static uint32_t letou32(const uint8_t* le)
{
	return	  ((uint32_t) le[0] << 0)
			| ((uint32_t) le[1] << 8)
			| ((uint32_t) le[2] << 16)
			| ((uint32_t) le[3] << 24);
}

static uint32_t u32tole(uint32_t host)
{
	uint8_t data[4] = {0,0,0,0};
	memcpy(&data, &host, sizeof(host));
	return	  ((uint32_t) data[0] << 0)
			| ((uint32_t) data[1] << 8)
			| ((uint32_t) data[2] << 16)
			| ((uint32_t) data[3] << 24);
}

#define NVRAM_HEADER_COUNTER_OFFSET 0
#define NVRAM_HEADER_SIZE_OFFSET 4
#define NVRAM_HEADER_DATA_CRC32_OFFSET 8
#define NVRAM_HEADER_HEADER_CRC32_OFFSET 12
#define NVRAM_HEADER_SIZE sizeof(uint32_t) * 4

#define NVRAM_ENTRY_KEY_LEN_OFFSET 0
#define NVRAM_ENTRY_VALUE_LEN_OFFSET 4
#define NVRAM_ENTRY_DATA_OFFSET 8
#define NVRAM_ENTRY_HEADER_SIZE sizeof(uint32_t) * 2

uint32_t nvram_header_len()
{
	return NVRAM_HEADER_SIZE;
}

int nvram_validate_header(const uint8_t* data, uint32_t len, struct nvram_header* hdr)
{
	if (len < NVRAM_HEADER_SIZE) {
		return -NVRAM_ERROR_INVALID;
	}

	uint32_t crc = calc_crc32(data, NVRAM_HEADER_HEADER_CRC32_OFFSET);
	uint32_t hdr_crc = letou32(data + NVRAM_HEADER_HEADER_CRC32_OFFSET);
	if (crc != hdr_crc) {
		return -NVRAM_ERROR_CRC;
	}

	hdr->counter = letou32(data + NVRAM_HEADER_COUNTER_OFFSET);
	hdr->data_len = letou32(data + NVRAM_HEADER_SIZE_OFFSET);
	hdr->data_crc32 = letou32(data + NVRAM_HEADER_DATA_CRC32_OFFSET);
	hdr->header_crc32 = hdr_crc;

	return 0;
}

// returns 0 for ok or negative nvram_error for error
static int validate_entry(const uint8_t* data, uint32_t len, struct nvram_entry* entry)
{
	//Minimum possible size of an entry where both key and value are 1 char each
	const uint32_t min_size = NVRAM_ENTRY_HEADER_SIZE + 2;

	if (len < min_size) {
		return -NVRAM_ERROR_ILLEGAL;
	}

	const uint32_t key_len = letou32(data + NVRAM_ENTRY_KEY_LEN_OFFSET);
	const uint32_t value_len = letou32(data + NVRAM_ENTRY_VALUE_LEN_OFFSET);
	const uint32_t required_len = key_len + value_len + NVRAM_ENTRY_HEADER_SIZE;
	if (required_len > len) {
		return -NVRAM_ERROR_ILLEGAL;
	}

	entry->key = (uint8_t*) data + NVRAM_ENTRY_DATA_OFFSET;
	entry->key_len = key_len;
	entry->value = (uint8_t*) data + NVRAM_ENTRY_DATA_OFFSET + key_len;
	entry->value_len = value_len;

	return 0;
}

static uint32_t entry_size(const struct nvram_entry* entry)
{
	return NVRAM_ENTRY_HEADER_SIZE + entry->key_len + entry->value_len;
}

int nvram_validate_data(const uint8_t* data, uint32_t len, const struct nvram_header* hdr)
{
	if (len < hdr->data_len) {
		return -NVRAM_ERROR_INVALID;
	}

	uint32_t crc = calc_crc32(data, hdr->data_len);
	if (crc != hdr->data_crc32) {
		return -NVRAM_ERROR_CRC;
	}

	for (uint32_t i = 0; i < hdr->data_len;) {
		uint32_t remaining = hdr->data_len - i;
		struct nvram_entry entry;
		int r = validate_entry(data + i, remaining, &entry);
		if (r) {
			return r;
		}
		i += entry_size(&entry);
	}

	return 0;
}

int nvram_deserialize(struct nvram_list** list, const uint8_t* data, uint32_t len, const struct nvram_header* hdr)
{
	if (len < hdr->data_len) {
		return -NVRAM_ERROR_INVALID;
	}
	if (*list) {
		return -NVRAM_ERROR_INVALID;
	}

	struct nvram_list *_list = NULL;
	int r = 0;
	for (uint32_t i = 0; i < hdr->data_len;) {
		uint32_t remaining = hdr->data_len - i;
		struct nvram_entry entry;
		r = validate_entry(data + i, remaining, &entry);
		if (r) {
			break;
		}
		i += entry_size(&entry);
		r = nvram_list_set(&_list, &entry);
		if (r) {
			break;
		}
	}

	if (r) {
		destroy_nvram_list(list);
	}
	else {
		*list = _list;
	}

	return r;
}

uint32_t nvram_serialize_size(const struct nvram_list* list)
{
	uint32_t size = NVRAM_HEADER_SIZE;
	if (list) {
		for (struct nvram_list* cur = (struct nvram_list*) list; cur != NULL; cur = cur->next) {
			size += NVRAM_ENTRY_HEADER_SIZE + cur->entry->key_len + cur->entry->value_len;
		}
	}
	return size;
}

static uint32_t write_entry(uint8_t* data, const struct nvram_entry* entry)
{
	const uint32_t le_key_len = u32tole(entry->key_len);
	const uint32_t le_value_len = u32tole(entry->value_len);
	memcpy(data + NVRAM_ENTRY_KEY_LEN_OFFSET, &le_key_len, sizeof(le_key_len));
	memcpy(data + NVRAM_ENTRY_VALUE_LEN_OFFSET, &le_value_len, sizeof(le_value_len));
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET, entry->key, entry->key_len);
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET + entry->key_len, entry->value, entry->value_len);
	return entry->key_len + entry->value_len + NVRAM_ENTRY_HEADER_SIZE;
}

static void write_header(uint8_t* data, struct nvram_header* hdr)
{
	const uint32_t le_counter = u32tole(hdr->counter);
	const uint32_t le_data_len = u32tole(hdr->data_len);
	const uint32_t le_data_crc32 = u32tole(hdr->data_crc32);
	memcpy(data + NVRAM_HEADER_COUNTER_OFFSET, &le_counter, sizeof(le_counter));
	memcpy(data + NVRAM_HEADER_SIZE_OFFSET, &le_data_len, sizeof(le_data_len));
	memcpy(data + NVRAM_HEADER_DATA_CRC32_OFFSET, &le_data_crc32, sizeof(le_data_crc32));
	hdr->header_crc32 = calc_crc32(data, NVRAM_HEADER_HEADER_CRC32_OFFSET);
	uint32_t le_header_crc32 = hdr->header_crc32;
	memcpy(data + NVRAM_HEADER_HEADER_CRC32_OFFSET, &le_header_crc32, sizeof(le_header_crc32));
}

uint32_t nvram_serialize(const struct nvram_list* list, uint8_t* data, uint32_t len, struct nvram_header* hdr)
{
	uint32_t required_size = nvram_serialize_size(list);
	if (len < required_size) {
		return 0;
	}

	uint32_t pos = NVRAM_HEADER_SIZE;
	for (struct nvram_list* cur = (struct nvram_list*) list; cur; cur = cur->next) {
		pos += write_entry(data + pos, cur->entry);
	}

	hdr->data_len = pos - NVRAM_HEADER_SIZE;
	hdr->data_crc32 = calc_crc32(data + NVRAM_HEADER_SIZE, hdr->data_len);

	write_header(data, hdr);

	return pos;
}

uint8_t* nvram_it_begin(const uint8_t* data, uint32_t len, const struct nvram_header* hdr)
{
	if (len < hdr->data_len) {
		return NULL;
	}
	return (uint8_t*) data;
}

uint8_t* nvram_it_next(const uint8_t* it)
{
	struct nvram_entry entry;
	validate_entry(it, UINT32_MAX, &entry);
	return (uint8_t*) it + NVRAM_ENTRY_HEADER_SIZE + entry.key_len + entry.value_len;
}

uint8_t* nvram_it_end(const uint8_t* data, uint32_t len, const struct nvram_header* hdr)
{
	if (len < hdr->data_len) {
		return NULL;
	}
	return (uint8_t*) data + hdr->data_len;
}

void nvram_it_deref(const uint8_t* it, struct nvram_entry* entry)
{
	validate_entry(it, UINT32_MAX, entry);
}

static void validate_section(struct nvram_section* section, const uint8_t* data, uint32_t len)
{
	int r = nvram_validate_header(data, len, &section->hdr);
	if (!r) {
		section->state |= NVRAM_STATE_HEADER_VERIFIED;
		if (len >= nvram_header_len()) {
			r = nvram_validate_data(data + nvram_header_len(), len - nvram_header_len(), &section->hdr);
			if (!r) {
				section->state |= NVRAM_STATE_DATA_VERIFIED;
			}
		}
	}
}

static enum nvram_active find_active(const struct nvram_section* section_a, const struct nvram_section* section_b)
{
	if (section_a->state == NVRAM_STATE_ALL_VERIFIED && (section_a->hdr.counter > section_b->hdr.counter)) {
		return NVRAM_ACTIVE_A;
	}
	else
	if (section_b->state == NVRAM_STATE_ALL_VERIFIED) {
		return NVRAM_ACTIVE_B;
	}
	else {
		return NVRAM_ACTIVE_NONE;
	}
}

void nvram_init_transaction(struct nvram_transaction* trans, const uint8_t* data_a, uint32_t len_a, const uint8_t* data_b, uint32_t len_b)
{
	validate_section(&trans->section_a, data_a, len_a);
	validate_section(&trans->section_b, data_b, len_b);
	trans->active = find_active(&trans->section_a, &trans->section_b);
}

enum nvram_active nvram_next_transaction(const struct nvram_transaction* trans, struct nvram_header* hdr)
{
	switch (trans->active) {
		case NVRAM_ACTIVE_A:
			hdr->counter = trans->section_a.hdr.counter + 1;
			return NVRAM_ACTIVE_B;
		case NVRAM_ACTIVE_B:
			hdr->counter = trans->section_b.hdr.counter + 1;
			return NVRAM_ACTIVE_A;
		default:
			hdr->counter = 1;
			return NVRAM_ACTIVE_A;
	}
}

