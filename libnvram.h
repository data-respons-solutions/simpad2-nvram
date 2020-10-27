#ifndef _LIBNVRAM_H_
#define _LIBNVRAM_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __UBOOT__
#include <stdint.h>
#include <inttypes.h>
#endif
#include "crc32.h"
/*
 * Serialized data is stored on nvram in two separate sections where section format is:
 *
 * counter|len|data_crc32|hdr_crc32|serialized_data
 *
 * uint32_t counter = Counter iterated for each write
 * uint32_t len = Length of serialized data
 * uint32_t data_crc32 = crc32 for serialized data.
 * uint32_t hdr_crc32 = crc32 for serialized header.
 * uint8_t[] data = serialized data
 *
 *
 *
 * Serialized data is stored as key/value entries in format:
 * key_len|value_len|key|value
 *
 * uint32_t key_len = Length of key string
 * uint32_t value_len = Lenght of value string
 * uint8_t[] key = Key string NOT including null terminator
 * uint8_t[] value = Value string NOT including null terminator
 */

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
 *   negative errno for error
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
uint32_t nvram_header_len();

/*
 * Validate and return header from data buffer
 *
 * @returns
 *   0 for valid
 *   negative errno for error
 *     -EINVAL: invalid argument
 *     -EFAULT: crc32 mismatch
 */
int nvram_validate_header(const uint8_t* data, uint32_t len, struct nvram_header* hdr);

/*
 * Validate data as described by hdr
 *
 * @returns
 *   0 for valid
 *   negative errno for error
 *     -EINVAL: invalid argument
 *     -EFAULT: crc32 mismatch or data invalid
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
 *  Negative errno for error
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

#ifdef __cplusplus
}
#endif

#endif // _LIBNVRAM_H_
