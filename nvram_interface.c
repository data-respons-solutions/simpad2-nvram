#include "nvram_interface.h"

const char* nvram_section_str(enum nvram_section_name section)
{
	switch(section) {
	case NVRAM_SECTION_A:
		return "A";
	case NVRAM_SECTION_B:
		return "B";
	default:
		return "SECTION_UNKNOWN";
	}
}
