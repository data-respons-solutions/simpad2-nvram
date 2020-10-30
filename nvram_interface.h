#ifndef _NVRAM_INTERFACE_H_
#define _NVRAM_INTERFACE_H_

#include <stdint.h>
#include <stddef.h>

/* Should be defined and allocate by nvram_interface_init.
 * nvram framework will call nvram_interface_destroy
 */
struct nvram_device;

/*
 * Initialize nvram interface
 *
 * @params
 *   dev: device
 *   section: String (i.e. path) for section A. The pointer must remain valid until
 *            nvram_interface_destroy is called.
 *
 * @returns
 *   0 for success
 *   negative errno for error
 */
int nvram_interface_init(struct nvram_device** dev, const char* section);

/*
 * Free allocated resources
 */
void nvram_interface_destroy(struct nvram_device** dev);

/*
 * Get size needed to be read
 *
 * @params
 *   dev: device
 *   size: size returned
 *
 * @returns
 *   0 for success
 *   negative errno for error
 */
int nvram_interface_size(struct nvram_device* dev, size_t* size);

/*
 * Read from nvram device into buffer
 *
 * @params
 *   dev: device
 *   buf: Read buffer
 *   size: Size of read buffer
 *
 * @returns
 *   0 for success (All "size" bytes read)
 *   negative errno for error
 */
int nvram_interface_read(struct nvram_device* dev, uint8_t* buf, size_t size);

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
int nvram_interface_write(struct nvram_device* dev, const uint8_t* buf, size_t size);

/*
 * Get section string from interface
 *
 * @params
 *   priv: private data
 *
 * @returns
 *   pointer to path string
 *   NULL if unavailable
 */
const char* nvram_interface_section(const struct nvram_device* dev);

#endif // _NVRAM_INTERFACE_H_
