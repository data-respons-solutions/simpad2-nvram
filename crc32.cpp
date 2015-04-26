#include "crc32.h"

//See crc32.h for copyright information


uint32_t computeCRC(uint32_t a_OldCRC, uint8_t *a_Data, uint32_t a_Length)
{
	a_OldCRC = a_OldCRC ^ 0xffffffffL;
	while (a_Length > 0)
	{
		a_OldCRC = CRC_TABLE[((a_OldCRC & 0xff) ^ (*a_Data++)) & 0xff] ^ (a_OldCRC >> 8);
		a_Length--;
	}
	return a_OldCRC ^ 0xffffffffL;
}

