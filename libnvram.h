#ifndef _LIBNVRAM_H_
#define _LIBNVRAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * This is a library for serializing key value pairs into non-volatile storage.
 * Keys used shall be unique. If identical keys are parsed then the last key
 * will take precedence.
 *
 * Serialized format:
 *
 *             HEADER              |                   DATA
 * -------------------------------------------------------------------------
 *                                 |            entry1           | entry..N
 * counter|len|data_crc32|hdr_crc32|key_len|value_len| key |value|........
 *   u32  |u32|    u32   |    u32  |  u32  |   u32   |u8..N|u8..N|........
 * -------------------------------------------------------------------------
 *
 * Header:
 * counter = Value decided by caller
 * len = Length of DATA section
 * data_crc32 = crc32 of DATA section
 * hdr_crc32 = crc32 of header, not including self
 *
 * Entry:
 * key_len = length of key
 * value_len = length of value
 * key = key bytes
 * value = value bytes
 */

enum nvram_error {
	NVRAM_ERROR_INVALID = 1,  // invalid argument
	NVRAM_ERROR_NOMEM,        // memory allocation failed
	NVRAM_ERROR_CRC,          // crc mismatch
	NVRAM_ERROR_ILLEGAL,      // illegal format
};

struct nvram_entry {
	uint8_t *key;
	uint32_t key_len;
	uint8_t *value;
	uint32_t value_len;
};

struct nvram_list {
	struct nvram_entry *entry;
	struct nvram_list *next;
};

/*
 * Set entry. Entry with identical key will be overwritten.
 * Entry is copied to list.
 *
 * @returns
 *   0 for success
 *   negative nvram_error for error
 */

int nvram_list_set(struct nvram_list** list, const struct nvram_entry* entry);

/*
 * Get value with key from list
 *
 * @returns
 *   Pointer to entry in list
 *   NULL if not found
 */
struct nvram_entry* nvram_list_get(const struct nvram_list* list, const uint8_t* key, uint32_t key_len);

/*
 * Remove entry.
 *
 * @returns
 * 1 if removed, 0 if not found
 */

int nvram_list_remove(struct nvram_list** list, const uint8_t* key, uint32_t key_len);

/*
 * Destroy nvram list
 */
void destroy_nvram_list(struct nvram_list** list);

struct nvram_header {
	uint32_t counter;
	uint32_t data_len;
	uint32_t data_crc32;
	uint32_t header_crc32;
};

/*
 * Returns length of serialized nvram header.
 * Useful if first validating header before reading data.
 */
uint32_t nvram_header_len(void);

/*
 * Validate and return header from data buffer
 *
 * @returns
 *   0 for valid
 *   negative nvram_error for error
 */
int nvram_validate_header(const uint8_t* data, uint32_t len, struct nvram_header* hdr);

/*
 * Validate data as described by hdr
 *
 * @returns
 *   0 for valid
 *   negative nvram_error for error
 */
int nvram_validate_data(const uint8_t* data, uint32_t len, const struct nvram_header* hdr);

/*
 * Returns nvram_list from validated data as described by hdr.
 * Partial list will not be returned if errors are detected.
 *
 * nvram_list should be destroyed by caller, see destroy_nvram_list()
 *
 * @returns
 *  0 for success
 *  Negative nvram_error for error
 */
int nvram_deserialize(struct nvram_list** list, const uint8_t* data, uint32_t len, const struct nvram_header* hdr);

/*
 * Returns size needed for serializing list.
 * Useful for allocating buffer for nvram_serialize().
 */
uint32_t nvram_serialize_size(const struct nvram_list* list);

/*
 * Create serialized data buffer from list.
 * Reads counter value from header.
 * Returns data_len, data_crc32 and header_crc32 in hdr.

 * @returns
 * Bytes used
 * 0 for error (Most likely buffer too small)
 */
uint32_t nvram_serialize(const struct nvram_list* list, uint8_t* data, uint32_t len, struct nvram_header* hdr);

/*
 *  Iterate over validated data as described by header
 *  Dereferencing end iterator is undefined behavior.
 *
 *  This is useful in environments where dynamic allocation for nvram_list is not possible.
 */
uint8_t* nvram_it_begin(const uint8_t* data, uint32_t len, const struct nvram_header* hdr);
uint8_t* nvram_it_next(const uint8_t* it);
uint8_t* nvram_it_end(const uint8_t* data, uint32_t len, const struct nvram_header* hdr);
void nvram_it_deref(const uint8_t* it, struct nvram_entry* entry);

/*
 * Transactional writes are a typical use case. We provide helper functions.
 *
 * The transaction API will iterate nvram_header->counter for each write
 * which indicates the last used section.
 * When nvram_header->counter == UINT32_MAX a counter reset is performed
 * requiring a write to both sections.
 */
enum nvram_state {
	NVRAM_STATE_UNKNOWN         = 0,
	NVRAM_STATE_HEADER_VERIFIED = 1 << 0,
	NVRAM_STATE_HEADER_CORRUPT  = 1 << 1,
	NVRAM_STATE_DATA_VERIFIED   = 1 << 2,
	NVRAM_STATE_DATA_CORRUPT    = 1 << 3,
	NVRAM_STATE_ALL_VERIFIED    = NVRAM_STATE_HEADER_VERIFIED | NVRAM_STATE_DATA_VERIFIED,
};

struct nvram_section {
	struct nvram_header hdr;
	enum nvram_state state;  // Current state of section flag
};

enum nvram_active {
	NVRAM_ACTIVE_NONE = 0,
	NVRAM_ACTIVE_A    = 1 << 0,
	NVRAM_ACTIVE_B    = 1 << 1,
};

struct nvram_transaction {
	struct nvram_section section_a;
	struct nvram_section section_b;
	enum nvram_active active; // enum flag
};

/*
 * Validates header and data for both data_a and data_b.
 * Results returned in nvram_transaction.
 */
void nvram_init_transaction(struct nvram_transaction* trans, const uint8_t* data_a, uint32_t len_a, const uint8_t* data_b, uint32_t len_b);

/*
 * Describes which nvram section next update should be written to.
 * Will always contain either NVRAM_OPERATION_WRITE_A or NVRAM_OPERATION_WRITE_B.
 * After performing write operation to above, if NVRAM_OPERATION_COUNTER_RESET is set,
 * also the second nvram section should be written to.
 */
enum nvram_operation {
	NVRAM_OPERATION_WRITE_A = 1 << 0,
	NVRAM_OPERATION_WRITE_B = 1 << 1,
	NVRAM_OPERATION_COUNTER_RESET = 1 << 2,
};

/*
 * Returns which operations should be performed when commiting changes to nvram.
 * The nvram_header returned from this function should be used for nvram_serialize call.
 */
enum nvram_operation nvram_next_transaction(const struct nvram_transaction* trans, struct nvram_header* hdr);

/*
 * Updates state of transaction. This function should be called after operations described by nvram_next_transaction
 * have been performed.
 * nvram_operation should be return value of nvram_next_transaction.
 * nvram_header should be the returned one from nvram_serialize call.
 */
void nvram_update_transaction(struct nvram_transaction* trans,  enum nvram_operation op, const struct nvram_header* hdr);

#ifdef __cplusplus
}
#endif

#endif // _LIBNVRAM_H_
