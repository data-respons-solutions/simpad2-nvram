#ifndef _LIBNVRAM_H_
#define _LIBNVRAM_H_

#include <stdint.h>
#include <inttypes.h>
#include "crc32.h"

/*
 * Serialized nvram data is stored as key/value entries in format:
 *
 * key_len|value_len|key|value
 *
 * uint32_t key_len = Length of key string
 * uint32_t value_len = Lenght of value string
 * uint8_t[] key = Key string NOT including null terminator
 * uint8_t[] value = Value string NOT including null terminator
 */

// Size of key_len and value_len
#define NVRAM_ENTRY_HEADER_SIZE sizeof(uint32_t) * 2
//Minimum possible size of an entry where both key and value are 1 char each
#define NVRAM_ENTRY_MIN_SIZE NVRAM_ENTRY_HEADER_SIZE + 2

// Parsed nvram data stored as linked nodes
struct nvram_node {
	char* key;
	char* value;
	struct nvram_node* next;
};

/*
 * Destroy nvram node
 *
 * @params
 *   node: node to destroy
 */
void destroy_nvram_node(struct nvram_node* node);

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
int is_valid_nvram_entry(const uint8_t* data, uint32_t max_len, uint32_t* key_len, uint32_t* value_len);

/*
 * Create serialized data buffer from nodes
 *
 * @params
 *  node: node with entries
 *  data: Returned data buffer
 *  len:  Returned data buffer length
 *
 * @returns
 *  0 for success
 *  Negative errno for error
 */
int nvram_serialize (struct nvram_node* const node, uint8_t** data, uint32_t* len);
/*
 * Create nodes from serialized data buffer
 *
 * @params
 *  node: returns nodes with entries
 *  data: data buffer
 *  len:  data buffer length
 *
 * @returns
 *  0 for success
 *  Negative errno for error
 */
int nvram_deserialize(struct nvram_node** node, const uint8_t* buf, uint32_t len);

/*
 * Serialized data is stored on nvram in two separate sections where section format is:
 *
 * counter|len|crc32|serialized_data
 *
 * uint32_t counter = Counter iterated for each write
 * uint32_t len = Length of serialized data
 * uint32_t crc32 = crc32 for serialized data
 * uint8_t[] data = serialized data
 */

// Size of counter, len and crc32
#define NVRAM_HEADER_SIZE sizeof(uint32_t) * 3

/*
 * Verify buffer contains valid nvram section
 *
 * @params
 *   data: data buffer
 *   max_len: Length of buffer
 *   data_len: returns length of serialized data
 *
 * @returns
 *   0 for invalid, 1 for valid
 */

int is_valid_nvram_section(const uint8_t* data, uint32_t len, uint32_t* data_len);

#endif // _LIBNVRAM_H_
