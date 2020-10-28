#ifndef _CRC32_H_
#define _CRC32_H_

#ifdef __UBOOT__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

uint32_t calc_crc32(const uint8_t *data, uint32_t len);

#endif //_CRC32_H_
