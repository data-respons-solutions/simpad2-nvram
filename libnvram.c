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
#include <assert.h>
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

#define member_size(type, member) sizeof(((type *)0)->member)

#define HEADER_SIZE				24
#define HEADER_MAGIC_VALUE		0xb32c41b4
#define HEADER_MAGIC_OFFSET		0
#define HEADER_MAGIC_SIZE		4
#define HEADER_USER_OFFSET		4
#define HEADER_USER_SIZE		4
#define HEADER_TYPE_OFFSET		8
#define HEADER_TYPE_SIZE		1
#define HEADER_RSVD_OFFSET		9
#define HEADER_RSVD_SIZE		3
#define HEADER_LEN_OFFSET		12
#define HEADER_LEN_SIZE			4
#define HEADER_CRC32_OFFSET		16
#define HEADER_CRC32_SIZE		4
#define HEADER_HDR_CRC32_OFFSET	20
#define HEADER_HDR_CRC32_SIZE	4

static_assert(HEADER_SIZE == sizeof(struct libnvram_header), "libnvram_header size unexpected");
static_assert(HEADER_MAGIC_SIZE == member_size(struct libnvram_header, magic), "libnvram_header.magic size unexpected");
static_assert(HEADER_USER_SIZE == member_size(struct libnvram_header, user), "libnvram_header.user size unexpected");
static_assert(HEADER_TYPE_SIZE == member_size(struct libnvram_header, type), "libnvram_header.type size unexpected");
static_assert(HEADER_RSVD_SIZE == member_size(struct libnvram_header, reserved), "libnvram_header.reserved size unexpected");
static_assert(HEADER_LEN_SIZE == member_size(struct libnvram_header, len), "libnvram_header.len size unexpected");
static_assert(HEADER_CRC32_SIZE == member_size(struct libnvram_header, crc32), "libnvram_header.crc32 size unexpected");
static_assert(HEADER_HDR_CRC32_SIZE == member_size(struct libnvram_header, hdr_crc32), "libnvram_header.hdr_crc32 size unexpected");

#define LIST_HEADER_SIZE		8
#define LIST_MIN_SIZE			10 // Header + key/value of length 1 each
#define LIST_KEY_LEN_OFFSET		0
#define LIST_KEY_LEN_SIZE		4
#define LIST_VALUE_LEN_OFFSET	4
#define LIST_VALUE_LEN_SIZE		4
#define	LIST_DATA_OFFSET		8

static_assert(LIST_KEY_LEN_SIZE == member_size(struct libnvram_entry, key_len), "libnvram_entry.key_len size unexpected");
static_assert(LIST_VALUE_LEN_SIZE == member_size(struct libnvram_entry, value_len), "libnvram_entry.value_len size unexpected");

uint32_t libnvram_header_len(void)
{
	return HEADER_SIZE;
}

int libnvram_validate_header(const uint8_t* data, uint32_t len, struct libnvram_header* hdr)
{
	if (len < HEADER_SIZE) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	const uint32_t crc = calc_crc32(data, HEADER_HDR_CRC32_OFFSET);
	const uint32_t hdr_crc = letou32(data + HEADER_HDR_CRC32_OFFSET);
	if (crc != hdr_crc) {
		return -LIBNVRAM_ERROR_CRC;
	}

	const uint32_t magic = letou32(data + HEADER_MAGIC_OFFSET);
	if (magic != HEADER_MAGIC_VALUE) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	hdr->magic = magic;
	hdr->user = letou32(data + HEADER_USER_OFFSET);
	hdr->type = *(data + HEADER_TYPE_OFFSET);
	hdr->reserved[0] = 0;
	hdr->reserved[1] = 0;
	hdr->reserved[2] = 0;
	hdr->len = letou32(data + HEADER_LEN_OFFSET);
	hdr->crc32 = letou32(data + HEADER_CRC32_OFFSET);
	hdr->hdr_crc32 = hdr_crc;

	return 0;
}

// returns 0 for ok or negative libnvram_error for error
static int validate_entry(const uint8_t* data, uint32_t len, struct libnvram_entry* entry)
{
	if (len < LIST_MIN_SIZE) {
		return -LIBNVRAM_ERROR_ILLEGAL;
	}

	const uint32_t key_len = letou32(data + LIST_KEY_LEN_OFFSET);
	const uint32_t value_len = letou32(data + LIST_VALUE_LEN_OFFSET);
	const uint32_t required_len = key_len + value_len + LIST_HEADER_SIZE;
	if (required_len > len) {
		return -LIBNVRAM_ERROR_ILLEGAL;
	}

	entry->key = (uint8_t*) data + LIST_DATA_OFFSET;
	entry->key_len = key_len;
	entry->value = (uint8_t*) data + LIST_DATA_OFFSET + key_len;
	entry->value_len = value_len;

	return 0;
}

static uint32_t entry_size(const struct libnvram_entry* entry)
{
	return LIST_HEADER_SIZE + entry->key_len + entry->value_len;
}

int libnvram_validate_data(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->len) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	const uint32_t crc = calc_crc32(data, hdr->len);
	if (crc != hdr->crc32) {
		return -LIBNVRAM_ERROR_CRC;
	}

	for (uint32_t i = 0; i < hdr->len;) {
		uint32_t remaining = hdr->len - i;
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
	if (len < hdr->len || hdr->type != LIBNVRAM_TYPE_LIST || *list) {
		return -LIBNVRAM_ERROR_INVALID;
	}

	struct libnvram_list *_list = NULL;
	int r = 0;
	for (uint32_t i = 0; i < hdr->len;) {
		uint32_t remaining = hdr->len - i;
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
		destroy_libnvram_list(&_list);
	}
	else {
		*list = _list;
	}

	return r;
}

uint32_t libnvram_serialize_size(const struct libnvram_list* list)
{
	uint32_t size = HEADER_SIZE;
	if (list) {
		for (struct libnvram_list* cur = (struct libnvram_list*) list; cur != NULL; cur = cur->next) {
			size += entry_size(cur->entry);
		}
	}
	return size;
}

static void memcpy_u32_as_le(uint8_t* data, uint32_t value)
{
	const uint32_t le = u32tole(value);
	memcpy(data, &le, sizeof(le));
}

static uint32_t write_entry(uint8_t* data, const struct libnvram_entry* entry)
{
	memcpy_u32_as_le(data + LIST_KEY_LEN_OFFSET, entry->key_len);
	memcpy_u32_as_le(data + LIST_VALUE_LEN_OFFSET, entry->value_len);
	memcpy(data + LIST_DATA_OFFSET, entry->key, entry->key_len);
	memcpy(data + LIST_DATA_OFFSET + entry->key_len, entry->value, entry->value_len);
	return entry_size(entry);
}

static void write_header(uint8_t* data, struct libnvram_header* hdr)
{
	memcpy_u32_as_le(data + HEADER_MAGIC_OFFSET, hdr->magic);
	memcpy_u32_as_le(data + HEADER_USER_OFFSET, hdr->user);
	data[HEADER_TYPE_OFFSET] = hdr->type;
	memset(data + HEADER_RSVD_OFFSET, 0, HEADER_RSVD_SIZE);
	memcpy_u32_as_le(data + HEADER_LEN_OFFSET, hdr->len);
	memcpy_u32_as_le(data + HEADER_CRC32_OFFSET, hdr->crc32);
	hdr->hdr_crc32 = calc_crc32(data, HEADER_HDR_CRC32_OFFSET);
	memcpy_u32_as_le(data + HEADER_HDR_CRC32_OFFSET, hdr->hdr_crc32);
}

uint32_t libnvram_serialize(const struct libnvram_list* list, uint8_t* data, uint32_t len, struct libnvram_header* hdr)
{
	if ((hdr->type != LIBNVRAM_TYPE_LIST) || !data) {
		return 0;
	}

	const uint32_t required_size = libnvram_serialize_size(list);
	if (len < required_size) {
		return 0;
	}

	uint32_t pos = HEADER_SIZE;
	for (struct libnvram_list* cur = (struct libnvram_list*) list; cur; cur = cur->next) {
		pos += write_entry(data + pos, cur->entry);
	}

	hdr->magic = HEADER_MAGIC_VALUE;
	hdr->len = pos - HEADER_SIZE;
	hdr->crc32 = calc_crc32(data + HEADER_SIZE, hdr->len);

	write_header(data, hdr);

	return pos;
}

uint8_t* libnvram_it_begin(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->len) {
		return NULL;
	}
	return (uint8_t*) data;
}

uint8_t* libnvram_it_next(const uint8_t* it)
{
	struct libnvram_entry entry;
	validate_entry(it, UINT32_MAX, &entry);
	return (uint8_t*) it + LIST_HEADER_SIZE + entry.key_len + entry.value_len;
}

uint8_t* libnvram_it_end(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr)
{
	if (len < hdr->len) {
		return NULL;
	}
	return (uint8_t*) data + hdr->len;
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
		if (section_a->hdr.user == section_b->hdr.user) {
			return LIBNVRAM_ACTIVE_A | LIBNVRAM_ACTIVE_B;
		}
		else
		if (section_a->hdr.user > section_b->hdr.user) {
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
		hdr->user = trans->section_a.hdr.user + 1;
	}
	else
	if ((trans->active & LIBNVRAM_ACTIVE_B) == LIBNVRAM_ACTIVE_B) {
		op = LIBNVRAM_OPERATION_WRITE_A;
		hdr->user = trans->section_b.hdr.user + 1;
	}
	else {
		op = LIBNVRAM_OPERATION_WRITE_A;
		hdr->user = 1;
	}

	if (hdr->user == UINT32_MAX) {
		hdr->user = 1;
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
