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
 * counter|len|crc32|serialized_data
 *
 * uint32_t counter = Counter iterated for each write
 * uint32_t len = Length of serialized data
 * uint32_t crc32 = crc32 for serialized data. This field zeroed during calculation
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

/*
 * Verify buffer contains valid nvram section
 *
 * @params
 *   data: data buffer
 *   max_len: Length of buffer
 *   data_len: returns length of serialized data
 *   counter: returns nvram section header counter
 *
 * @returns
 *   0 for invalid, 1 for valid
 */
int is_valid_nvram_section(const uint8_t* data, uint32_t len, uint32_t* data_len, uint32_t* counter);

/*
 * Create nodes from serialized data buffer.
 * This function expects nvram section is valid.
 *
 * @params
 *  list: returns list with entries (Resource should be released with destroy_nvram_node() by caller)
 *  data: data buffer
 *  len:  data buffer length
 *
 * @returns
 *  0 for success
 *  Negative errno for error
 */
int nvram_section_deserialize(struct nvram_list** list, const uint8_t* data, uint32_t len);

/*
 * Returns size needed for serialized buffer
 *
 * @params
 *   list: list with entires
 *   size: returns buffer size needed
 *
 * @returns
 *   0 for success
 *   Negative errno for error
 */
int nvram_section_serialize_size(const struct nvram_list* list, uint32_t* size);

/*
 * Create serialized data buffer from nodes
 *
 * @params
 *  list: list with entries
 *  counter: Value of counter to insert into section header
 *  data: data buffer
 *  size: size of buffer
 *
 * @returns
 *  0 for success
 *  Negative errno for error
 */
int nvram_section_serialize(const struct nvram_list* list, uint32_t counter, uint8_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // _LIBNVRAM_H_
