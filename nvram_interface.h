#ifndef _NVRAM_INTERFACE_H_
#define _NVRAM_INTERFACE_H_

#include <stdint.h>
#include <stddef.h>

enum nvram_section {
	NVRAM_SECTION_UNKNOWN = 0,
	NVRAM_SECTION_A,
	NVRAM_SECTION_B,
};

struct nvram_interface_priv;

// size 0 if not exist and possible could be created
int nvram_interface_init(struct nvram_interface_priv** priv, const char* section_a, const char* section_b);
int nvram_interface_size(struct nvram_interface_priv* priv, enum nvram_section section, size_t* size);
int nvram_interface_read(struct nvram_interface_priv* priv, enum nvram_section section, uint8_t* buf, size_t size);
int nvram_interface_write(struct nvram_interface_priv* priv, enum nvram_section section, const uint8_t* buf, size_t size);
const char* nvram_interface_path(const struct nvram_interface_priv* priv, enum nvram_section section);
const char* nvram_section_str(enum nvram_section section);

#endif // _NVRAM_INTERFACE_H_
