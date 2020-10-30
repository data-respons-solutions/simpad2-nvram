#ifndef _NVRAM_INTERFACE_H_
#define _NVRAM_INTERFACE_H_

#include <stdint.h>
#include <stddef.h>

/*
 * DISABLING TRANSACTIONAL BASED READING -- USING SINGLE SECTION
 * If the interface does not require transactional based storage then
 * the behavior can be disabled by:
 *   nvram_interface_path => Always returns path to the one section in use
 *   nvram_interface_size => Always returns size 0 for section not in use
 */

enum nvram_section_name {
	NVRAM_SECTION_UNKNOWN = 0,
	NVRAM_SECTION_A,
	NVRAM_SECTION_B,
};

/* Should be defined and allocated, if needed, in interface file.
 * Freeing will be handled by nvram framework */
struct nvram_interface_priv;

/*
 * Initialize nvram interface
 *
 * @params
 *   priv: private data
 *   section_a: String (i.e. path) for section A. The pointer must remain valid during program execution.
 *   section_b: String (i.e. path) for section B. The pointer must remain valid during program execution.
 *
 * @returns
 *   0 for success
 *   negative errno for error
 */
int nvram_interface_init(struct nvram_interface_priv** priv, const char* section_a, const char* section_b);

/*
 * Get size needed to be read
 *
 * @params
 *   priv: private data
 *   section: Section to operate on
 *   size: size returned
 *
 * @returns
 *   0 for success
 *   negative errno for error
 */
int nvram_interface_size(struct nvram_interface_priv* priv, enum nvram_section_name section, size_t* size);

/*
 * Read from nvram device into buffer
 *
 * @params
 *   priv: private data
 *   section: Section to operate on
 *   buf: Read buffer
 *   size: Size of read buffer
 *
 * @returns
 *   0 for success (All "size" bytes read)
 *   negative errno for error
 */
int nvram_interface_read(struct nvram_interface_priv* priv, enum nvram_section_name section, uint8_t* buf, size_t size);

/*
 * Write from buffer into nvram device
 *
 * @params
 *   priv: private data
 *   section: Section to operate on
 *   buf: write buffer
 *   size: Size of write buffer
 *
 * @returns
 *   0 for success (All "size" bytes written)
 *   negative errno for error
 */
int nvram_interface_write(struct nvram_interface_priv* priv, enum nvram_section_name section, const uint8_t* buf, size_t size);

/*
 * Resolve section to path
 *
 * @params
 *   priv: private data
 *   section: Section
 *
 * @returns
 *   pointer to path string
 *   NULL if unavailable
 */
const char* nvram_interface_path(const struct nvram_interface_priv* priv, enum nvram_section_name section);

/*
 * Resolve enum nvram_section_name to string
 *
 * @params
 *   section: Section
 *
 * @returns
 *   pointer to section string
 */
const char* nvram_section_str(enum nvram_section_name section);

#endif // _NVRAM_INTERFACE_H_
