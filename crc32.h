#ifndef _CRC32_H_
#define _CRC32_H_

#ifdef __UBOOT__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/*
  This file is derived from crc32.c from the u-boot source tree which in turn has
  derived it's code from the crc32.c from the zlib-1.1.3 distribution
  by Jean-loup Gailly and Mark Adler.


This is the copyright for the orignal work:

    Copyright (C) 1995-2005 Jean-loup Gailly and Mark Adler

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.

    Jean-loup Gailly        Mark Adler
    jloup@gzip.org          madler@alumni.caltech.edu

*/

uint32_t calc_crc32(uint32_t old_crc, const uint8_t *data, uint32_t len);

#endif //_CRC32_H_
