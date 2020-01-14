#ifndef _NVRAM_H_
#define _NVRAM_H_

#include <stdint.h>
#include "libnvram/libnvram.h"
#include "nvram_interface.h"

#define NVRAM_INITIALIZER {NVRAM_SECTION_UNKNOWN, 0, NULL}

struct nvram {
	enum nvram_section section;
	uint32_t counter;
	struct nvram_interface_priv *priv;
};

int nvram_init(struct nvram* nvram, struct nvram_list* list, const char* section_a, const char* section_b);
int nvram_commit(struct nvram* nvram, const struct nvram_list* list);
int nvram_close(struct nvram* nvram);

#endif // _NVRAM_H_
