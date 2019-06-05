/*
 * EepromVpdNoFS.h
 *
 *  Created on: Jun 8, 2016
 *      Author: asi
 */

#ifndef EEPROM_VPD_NO_FS_H_
#define EEPROM_VPD_NO_FS_H_
#include <string>
#include "vpdstorage.h"

#define EEPROM_MIN_ERASE_SIZE	0x10000		// minimum eraseblock size 64k
#define EEPROM_START_ADDR 		0x0			// start address of /dev/mtd1
#define EEPROM_MSG_LEN 			0x2			// first 2 bytes getting messages length
#define EEPROM_EMPTY 			0xFFFF		// eeprom empty, returns 0xFFFF for the first two bytes

class EepromVpdNoFS : public VpdStorage
{
public:
	EepromVpdNoFS(const std::string& fileName, int eesize, const std::string& wpGpio);
	virtual ~EepromVpdNoFS();

	bool load(std::list<std::string>& to);
	bool store(const std::list<std::string>& strings);
	int erase_eeprom(int fd, u_int32_t offset, u_int32_t bytes);
private:
	std::string _filePath;
	std::string _wpGpioPath; 	// Optional write protect (low) GPIO number
	int _eeSize;	// Size in bytes
};

#endif /* EEPROM_VPD_NO_FS_H_ */
