#ifndef _LIBNVRAM_H_
#define _LIBNVRAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * This is a library for serializing key value pairs into non-volatile storage.
 * If identical keys are parsed then the last key will take precedence.
 *
 * Serialized format -- all values little endian:
 *
 * HEADER
 * ======
 * u32: magic: magic value 0xb32c41b4
 * u32: user: user defined value
 * u8 : type: type of data section
 *            available types:
 *            0: list
 * u8 : reserved
 * u8 : reserved
 * u8 : reserved
 * u32: len: length of data section
 * u32: crc32: crc32 of data section
 * u32: hdr_crc32: crc32 of header, not including this field
 *
 * DATA
 * ====
 * LIST
 * ----
 * any number of entries of format:
 * u32: key_len: length of key
 * u32: value_len: length of value
 * u8*: key: array of length key_len
 * u8*: value: array of length value_len
 */

enum libnvram_error {
	LIBNVRAM_ERROR_INVALID = 1,  // invalid argument
	LIBNVRAM_ERROR_NOMEM,        // memory allocation failed
	LIBNVRAM_ERROR_CRC,          // crc mismatch
	LIBNVRAM_ERROR_ILLEGAL,      // illegal format
};

struct libnvram_entry {
	uint8_t *key;
	uint32_t key_len;
	uint8_t *value;
	uint32_t value_len;
};

struct libnvram_list {
	struct libnvram_entry *entry;
	struct libnvram_list *next;
};

/*
 * Size of list, number of entries.
 */
uint32_t libnvram_list_size(const struct libnvram_list* list);

/*
 * Set entry. Entry with identical key will be overwritten.
 * Entry is copied to list.
 *
 * @returns
 *   0 for success
 *   negative libnvram_error for error
 */
int libnvram_list_set(struct libnvram_list** list, const struct libnvram_entry* entry);

/*
 * Get value with key from list
 *
 * @returns
 *   Pointer to entry in list
 *   NULL if not found
 */
struct libnvram_entry* libnvram_list_get(const struct libnvram_list* list, const uint8_t* key, uint32_t key_len);

/*
 * Remove entry.
 *
 * @returns
 * 1 if removed, 0 if not found
 */
int libnvram_list_remove(struct libnvram_list** list, const uint8_t* key, uint32_t key_len);

/*
 * Destroy libnvram list
 */
void destroy_libnvram_list(struct libnvram_list** list);

enum libnvram_type {
	LIBNVRAM_TYPE_LIST = 0,
};

struct libnvram_header {
	uint32_t magic;
	uint32_t user;
	uint8_t type;
	uint8_t reserved[3];
	uint32_t len;
	uint32_t crc32;
	uint32_t hdr_crc32;
};

/*
 * Returns length of serialized nvram header.
 * Useful if first validating header before reading data.
 */
uint32_t libnvram_header_len(void);

/*
 * Validate and return header from data buffer
 *
 * @returns
 *   0 for valid
 *   negative libnvram_error for error
 */
int libnvram_validate_header(const uint8_t* data, uint32_t len, struct libnvram_header* hdr);

/*
 * Validate data as described by hdr
 *
 * @returns
 *   0 for valid
 *   negative libnvram_error for error
 */
int libnvram_validate_data(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr);

/*
 * Returns libnvram_list from validated data as described by hdr.
 * Partial list will not be returned if errors are detected.
 *
 * libnvram_list should be destroyed by caller, see destroy_libnvram_list()
 *
 * @returns
 *  0 for success
 *  Negative libnvram_error for error
 */
int libnvram_deserialize(struct libnvram_list** list, const uint8_t* data, uint32_t len, const struct libnvram_header* hdr);

/*
 * Returns size needed for serializing list.
 * Useful for allocating buffer for libnvram_serialize().
 *
 * Unsupported types will return 0.
 */
uint32_t libnvram_serialize_size(const struct libnvram_list* list, enum libnvram_type type);

/*
 * Create serialized data buffer from list.
 * Reads fields user and type from hdr.
 * Returns magic, type, len, crc32 and hdr_crc32 in hdr.

 * @returns
 * Bytes used
 * 0 for error (Most likely buffer too small)
 */
uint32_t libnvram_serialize(const struct libnvram_list* list, uint8_t* data, uint32_t len, struct libnvram_header* hdr);

/*
 *  Iterate over validated data as described by header
 *  Dereferencing end iterator is undefined behavior.
 *
 *  This is useful in environments where dynamic allocation for libnvram_list is not possible.
 */
uint8_t* libnvram_it_begin(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr);
uint8_t* libnvram_it_next(const uint8_t* it);
uint8_t* libnvram_it_end(const uint8_t* data, uint32_t len, const struct libnvram_header* hdr);
void libnvram_it_deref(const uint8_t* it, struct libnvram_entry* entry);

/*
 * Transactional writes are a typical use case. We provide helper functions.
 *
 * The transaction API will iterate libnvram_header->user as a counter for
 * each write which indicates the last used section.
 * When libnvram_header->user == UINT32_MAX a counter reset is performed
 * requiring a write to both sections.
 */
enum libnvram_state {
	LIBNVRAM_STATE_UNKNOWN         = 0,
	LIBNVRAM_STATE_HEADER_VERIFIED = 1 << 0,
	LIBNVRAM_STATE_HEADER_CORRUPT  = 1 << 1,
	LIBNVRAM_STATE_DATA_VERIFIED   = 1 << 2,
	LIBNVRAM_STATE_DATA_CORRUPT    = 1 << 3,
	LIBNVRAM_STATE_ALL_VERIFIED    = LIBNVRAM_STATE_HEADER_VERIFIED | LIBNVRAM_STATE_DATA_VERIFIED,
};

struct libnvram_section {
	struct libnvram_header hdr;
	enum libnvram_state state;  // Current state of section flag
};

enum libnvram_active {
	LIBNVRAM_ACTIVE_NONE = 0,
	LIBNVRAM_ACTIVE_A    = 1 << 0,
	LIBNVRAM_ACTIVE_B    = 1 << 1,
};

struct libnvram_transaction {
	struct libnvram_section section_a;
	struct libnvram_section section_b;
	enum libnvram_active active; // enum flag
};

/*
 * Validates header and data for both data_a and data_b.
 * Results returned in libnvram_transaction.
 */
void libnvram_init_transaction(struct libnvram_transaction* trans, const uint8_t* data_a, uint32_t len_a, const uint8_t* data_b, uint32_t len_b);

/*
 * Describes which nvram section next update should be written to.
 * Will always contain either LIBNVRAM_OPERATION_WRITE_A or LIBNVRAM_OPERATION_WRITE_B.
 * After performing write operation to above, if LIBNVRAM_OPERATION_COUNTER_RESET is set,
 * also the second nvram section should be written to.
 */
enum libnvram_operation {
	LIBNVRAM_OPERATION_WRITE_A = 1 << 0,
	LIBNVRAM_OPERATION_WRITE_B = 1 << 1,
	LIBNVRAM_OPERATION_COUNTER_RESET = 1 << 2,
};

/*
 * Returns which operations should be performed when commiting changes to nvram.
 * The libnvram_header returned from this function should be used for libnvram_serialize call.
 */
enum libnvram_operation libnvram_next_transaction(const struct libnvram_transaction* trans, struct libnvram_header* hdr);

/*
 * Updates state of transaction. This function should be called after operations described by libnvram_next_transaction
 * have been performed.
 * libnvram_operation should be return value of libnvram_next_transaction.
 * libnvram_header should be the returned one from libnvram_serialize call.
 */
void libnvram_update_transaction(struct libnvram_transaction* trans,  enum libnvram_operation op, const struct libnvram_header* hdr);

#ifdef __cplusplus
}
#endif

#endif // _LIBNVRAM_H_
