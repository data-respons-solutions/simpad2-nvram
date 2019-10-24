#ifndef _LIBNVRAM_H_
#define _LIBNVRAM_H_

#include <stdint.h>
#include <inttypes.h>
#include "crc32.h"

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
 *  node: returns nodes with entries (Resource should be released with destroy_nvram_node() by caller)
 *  data: data buffer
 *  len:  data buffer length
 *
 * @returns
 *  0 for success
 *  Negative errno for error
 */
int nvram_section_deserialize(struct nvram_node** node, const uint8_t* data, uint32_t len);

/*
 * Create serialized data buffer from nodes
 *
 * @params
 *  node: node with entries
 *  counter: Value of counter to insert into section header
 *  data: Returned data buffer
 *  len:  Returned data buffer length
 *
 * @returns
 *  0 for success
 *  Negative errno for error
 */
int nvram_section_serialize(struct nvram_node* node, uint32_t counter, uint8_t** data, uint32_t* len);

#endif // _LIBNVRAM_H_
