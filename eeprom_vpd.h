/*
 * EepromVpd.h
 *
 *  Created on: Apr 26, 2015
 *      Author: hcl
 */

#ifndef EEPROM_VPD_H_
#define EEPROM_VPD_H_
#include <string>
#include "vpdstorage.h"

class EepromVpd : public VpdStorage
{
public:
	EepromVpd(const std::string& fileName, int eesize, int wpGpio=-1);
	virtual ~EepromVpd();

	bool load(std::list<std::string>& to);
	bool store(const std::list<std::string>& strings);
private:
	std::string _filePath;
	int _wpGpio; 	// Optional write protect (low) GPIO number
	int _eeSize;	// Size in bytes
};

#endif /* EEPROM_VPD_H_ */
