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
 *   libnvram_entry (Resource should be released with destroy_libnvram_entry() by caller)
 *   NULL for failure.
 */
static struct libnvram_entry*
create_libnvram_entry(const uint8_t* key, uint32_t key_len, const uint8_t* value, uint32_t value_len)
{
	const size_t size = key_len + value_len + sizeof(struct libnvram_entry);
	struct libnvram_entry *entry = malloc(size);
	if (!entry) {
		return NULL;
	}
	entry->key = (uint8_t*) entry + sizeof(struct libnvram_entry);
	memcpy(entry->key, key, key_len);
	entry->key_len = key_len;

	entry->value = (uint8_t*) entry + sizeof(struct libnvram_entry) + key_len;
	memcpy(entry->value, value, value_len);
	entry->value_len = value_len;

	return entry;
}

static void destroy_libnvram_entry(struct libnvram_entry* entry)
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

int libnvram_list_set(struct libnvram_list** list, const struct libnvram_entry* entry)
{
	struct libnvram_list* prev = NULL;
	struct libnvram_list* cur = *list;
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

	struct libnvram_entry *new = create_libnvram_entry(entry->key, entry->key_len, entry->value, entry->value_len);
	if (!new) {
		return -LIBNVRAM_ERROR_NOMEM;
	}

	if (!cur) {
		// new entry
		cur = malloc(sizeof(struct libnvram_list));
		if (!cur) {
			destroy_libnvram_entry(new);
			return -LIBNVRAM_ERROR_NOMEM;
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
		destroy_libnvram_entry(cur->entry);
		cur->entry = new;
	}

	return 0;
}

struct libnvram_entry* libnvram_list_get(const struct libnvram_list* list, const uint8_t* key, uint32_t key_len)
{
	struct libnvram_list* cur = (struct libnvram_list*) list;
	while(cur) {
		if (!keycmp(cur->entry->key, cur->entry->key_len, key, key_len)) {
			return cur->entry;
		}
		cur = cur->next;
	}

	return NULL;
}

int libnvram_list_remove(struct libnvram_list** list, const uint8_t* key, uint32_t key_len)
{
	struct libnvram_list* prev = NULL;
	struct libnvram_list* cur = *list;
	while(cur) {
		if (!keycmp(cur->entry->key, cur->entry->key_len, key, key_len)) {
			destroy_libnvram_entry(cur->entry);
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

void destroy_libnvram_list(struct libnvram_list** list)
{
	struct libnvram_list *cur = *list;
	while (cur) {
		destroy_libnvram_entry(cur->entry);
		struct libnvram_list *prev = cur;
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

uint32_t libnvram_header_len(void)
{
	return NVRAM_HEADER_SIZE;
}

int libnvram_validate_header(const uint8_t* data, uint32_t len, struct libnvram_header* hdr)
{
	if (len < NVRAM_HEADER_SIZE) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	uint32_t crc = calc_crc32(data, NVRAM_HEADER_HEADER_CRC32_OFFSET);
	uint32_t hdr_crc = letou32(data + NVRAM_HEADER_HEADER_CRC32_OFFSET);
	if (crc != hdr_crc) {
		return -LIBNVRAM_ERROR_CRC;
	}

	hdr->counter = letou32(data + NVRAM_HEADER_COUNTER_OFFSET);
	hdr->data_len = letou32(data + NVRAM_HEADER_SIZE_OFFSET);
	hdr->data_crc32 = letou32(data + NVRAM_HEADER_DATA_CRC32_OFFSET);
	hdr->header_crc32 = hdr_crc;

	return 0;
}

// returns 0 for ok or negative libnvram_error for error
static int validate_entry(const uint8_t* data, uint32_t len, struct libnvram_entry* entry)
{
	//Minimum possible size of an entry where both key and value are 1 char each
	const uint32_t min_size = NVRAM_ENTRY_HEADER_SIZE + 2;

	if (len < min_size) {
		return -LIBNVRAM_ERROR_ILLEGAL;
	}

	const uint32_t key_len = letou32(data + NVRAM_ENTRY_KEY_LEN_OFFSET);
	const uint32_t value_len = letou32(data + NVRAM_ENTRY_VALUE_LEN_OFFSET);
	const uint32_t required_len = key_len + value_len + NVRAM_ENTRY_HEADER_SIZE;
	if (required_len > len) {
		return -LIBNVRAM_ERROR_ILLEGAL;
	}

	entry->key = (uint8_t*) data + NVRAM_ENTRY_DATA_OFFSET;
	entry->key_len = key_len;
	entry->value = (uint8_t*) data + NVRAM_ENTRY_DATA_OFFSET + key_len;
	entry->value_len = value_len;

	return 0;
}

static uint32_t entry_size(const struct libnvram_entry* entry)
{
	return NVRAM_ENTRY_HEADER_SIZE + entry->key_len + entry->value_len;
}

int libnvram_validate_data(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->data_len) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	uint32_t crc = calc_crc32(data, hdr->data_len);
	if (crc != hdr->data_crc32) {
		return -LIBNVRAM_ERROR_CRC;
	}

	for (uint32_t i = 0; i < hdr->data_len;) {
		uint32_t remaining = hdr->data_len - i;
		struct libnvram_entry entry;
		int r = validate_entry(data + i, remaining, &entry);
		if (r) {
			return r;
		}
		i += entry_size(&entry);
	}

	return 0;
}

int libnvram_deserialize(struct libnvram_list** list, const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->data_len) {
		return -LIBNVRAM_ERROR_INVALID;
	}
	if (*list) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	struct libnvram_list *_list = NULL;
	int r = 0;
	for (uint32_t i = 0; i < hdr->data_len;) {
		uint32_t remaining = hdr->data_len - i;
		struct libnvram_entry entry;
		r = validate_entry(data + i, remaining, &entry);
		if (r) {
			break;
		}
		i += entry_size(&entry);
		r = libnvram_list_set(&_list, &entry);
		if (r) {
			break;
		}
	}

	if (r) {
		destroy_libnvram_list(list);
	}
	else {
		*list = _list;
	}

	return r;
}

uint32_t libnvram_serialize_size(const struct libnvram_list* list)
{
	uint32_t size = NVRAM_HEADER_SIZE;
	if (list) {
		for (struct libnvram_list* cur = (struct libnvram_list*) list; cur != NULL; cur = cur->next) {
			size += NVRAM_ENTRY_HEADER_SIZE + cur->entry->key_len + cur->entry->value_len;
		}
	}
	return size;
}

static uint32_t write_entry(uint8_t* data, const struct libnvram_entry* entry)
{
	const uint32_t le_key_len = u32tole(entry->key_len);
	const uint32_t le_value_len = u32tole(entry->value_len);
	memcpy(data + NVRAM_ENTRY_KEY_LEN_OFFSET, &le_key_len, sizeof(le_key_len));
	memcpy(data + NVRAM_ENTRY_VALUE_LEN_OFFSET, &le_value_len, sizeof(le_value_len));
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET, entry->key, entry->key_len);
	memcpy(data + NVRAM_ENTRY_DATA_OFFSET + entry->key_len, entry->value, entry->value_len);
	return entry->key_len + entry->value_len + NVRAM_ENTRY_HEADER_SIZE;
}

static void write_header(uint8_t* data, struct libnvram_header* hdr)
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

uint32_t libnvram_serialize(const struct libnvram_list* list, uint8_t* data, uint32_t len, struct libnvram_header* hdr)
{
	uint32_t required_size = libnvram_serialize_size(list);
	if (len < required_size) {
		return 0;
	}

	uint32_t pos = NVRAM_HEADER_SIZE;
	for (struct libnvram_list* cur = (struct libnvram_list*) list; cur; cur = cur->next) {
		pos += write_entry(data + pos, cur->entry);
	}

	hdr->data_len = pos - NVRAM_HEADER_SIZE;
	hdr->data_crc32 = calc_crc32(data + NVRAM_HEADER_SIZE, hdr->data_len);

	write_header(data, hdr);

	return pos;
}

uint8_t* libnvram_it_begin(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->data_len) {
		return NULL;
	}
	return (uint8_t*) data;
}

uint8_t* libnvram_it_next(const uint8_t* it)
{
	struct libnvram_entry entry;
	validate_entry(it, UINT32_MAX, &entry);
	return (uint8_t*) it + NVRAM_ENTRY_HEADER_SIZE + entry.key_len + entry.value_len;
}

uint8_t* libnvram_it_end(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->data_len) {
		return NULL;
	}
	return (uint8_t*) data + hdr->data_len;
}

void libnvram_it_deref(const uint8_t* it, struct libnvram_entry* entry)
{
	validate_entry(it, UINT32_MAX, entry);
}

static void validate_section(struct libnvram_section* section, const uint8_t* data, uint32_t len)
{
	int r = libnvram_validate_header(data, len, &section->hdr);
	if (!r) {
		section->state |= LIBNVRAM_STATE_HEADER_VERIFIED;
	}
	else {
		section->state |= LIBNVRAM_STATE_HEADER_CORRUPT;
		return;
	}

	r = 1;
	if (len >= libnvram_header_len()) {
		r = libnvram_validate_data(data + libnvram_header_len(), len - libnvram_header_len(), &section->hdr);
	}
	if (!r) {
		section->state |= LIBNVRAM_STATE_DATA_VERIFIED;
	}
	else {
		section->state |= LIBNVRAM_STATE_DATA_CORRUPT;
	}
	return;
}

static enum libnvram_active find_active(const struct libnvram_section* section_a, const struct libnvram_section* section_b)
{
	const int is_verified_a = (section_a->state & LIBNVRAM_STATE_ALL_VERIFIED) == LIBNVRAM_STATE_ALL_VERIFIED;
	const int is_verified_b = (section_b->state & LIBNVRAM_STATE_ALL_VERIFIED) == LIBNVRAM_STATE_ALL_VERIFIED;
	if (is_verified_a && is_verified_b) {
		if (section_a->hdr.counter == section_b->hdr.counter) {
			return LIBNVRAM_ACTIVE_A | LIBNVRAM_ACTIVE_B;
		}
		else
		if (section_a->hdr.counter > section_b->hdr.counter) {
			return LIBNVRAM_ACTIVE_A;
		}
		else {
			return LIBNVRAM_ACTIVE_B;
		}
	}
	else
	if (is_verified_a) {
		return LIBNVRAM_ACTIVE_A;
	}
	else
	if (is_verified_b) {
		return LIBNVRAM_ACTIVE_B;
	}
	else {
		return LIBNVRAM_ACTIVE_NONE;
	}
}

void libnvram_init_transaction(struct libnvram_transaction* trans, const uint8_t* data_a, uint32_t len_a, const uint8_t* data_b, uint32_t len_b)
{
	validate_section(&trans->section_a, data_a, len_a);
	validate_section(&trans->section_b, data_b, len_b);
	trans->active = find_active(&trans->section_a, &trans->section_b);
}

enum libnvram_operation libnvram_next_transaction(const struct libnvram_transaction* trans, struct libnvram_header* hdr)
{
	enum libnvram_operation op = LIBNVRAM_OPERATION_WRITE_A;
	if ((trans->active & LIBNVRAM_ACTIVE_A) == LIBNVRAM_ACTIVE_A) {
		op = LIBNVRAM_OPERATION_WRITE_B;
		hdr->counter = trans->section_a.hdr.counter + 1;
	}
	else
	if ((trans->active & LIBNVRAM_ACTIVE_B) == LIBNVRAM_ACTIVE_B) {
		op = LIBNVRAM_OPERATION_WRITE_A;
		hdr->counter = trans->section_b.hdr.counter + 1;
	}
	else {
		op = LIBNVRAM_OPERATION_WRITE_A;
		hdr->counter = 1;
	}

	if (hdr->counter == UINT32_MAX) {
		hdr->counter = 1;
		op |= LIBNVRAM_OPERATION_COUNTER_RESET;
	}

	return op;
}

void libnvram_update_transaction(struct libnvram_transaction* trans,  enum libnvram_operation op, const struct libnvram_header* hdr)
{
	const int is_write_other = (op & LIBNVRAM_OPERATION_COUNTER_RESET) == LIBNVRAM_OPERATION_COUNTER_RESET;
	const int is_write_a = (op & LIBNVRAM_OPERATION_WRITE_A) == LIBNVRAM_OPERATION_WRITE_A || is_write_other;
	const int is_write_b = (op & LIBNVRAM_OPERATION_WRITE_B) == LIBNVRAM_OPERATION_WRITE_B || is_write_other;
	if (is_write_a) {
		memcpy(&trans->section_a.hdr, hdr, sizeof(struct libnvram_header));
		trans->section_a.state = LIBNVRAM_STATE_ALL_VERIFIED;
	}
	if (is_write_b) {
		memcpy(&trans->section_b.hdr, hdr, sizeof(struct libnvram_header));
		trans->section_b.state = LIBNVRAM_STATE_ALL_VERIFIED;
	}
	trans->active = find_active(&trans->section_a, &trans->section_b);
}
