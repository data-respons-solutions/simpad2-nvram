#ifndef _NVRAM_H_
#define _NVRAM_H_

#include <stdint.h>
#include "libnvram/libnvram.h"
#include "nvram_interface.h"

struct nvram;

int nvram_init(struct nvram** nvram, struct nvram_list* list, const char* section_a, const char* section_b);
int nvram_commit(struct nvram* nvram, const struct nvram_list* list);
int nvram_close(struct nvram** nvram);

#endif // _NVRAM_H_
