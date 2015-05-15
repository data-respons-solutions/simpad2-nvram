/*
 * MtdVpd.h
 *
 *  Created on: Apr 26, 2015
 *      Author: hcl
 */

#ifndef MTDVPD_H_
#define MTDVPD_H_
#include <string>
#include "vpdstorage.h"

class MtdVpd : public VpdStorage
{
public:
	MtdVpd(std::string& mtdPart, int mtdSize, int wpGpio=-1);
	virtual ~MtdVpd();

	uint8_t* load(int& size);
	bool store(const uint8_t *vpdData, int size);
private:
	int _vpdFd;
	std::string _filePath;
	uint8_t *_image;
	int _wpGpio; // Optional write protect (low) GPIO number
	int _mtdSize;	// Size in bytes of MTD block
};

#endif /* MTDVPD_H_ */
