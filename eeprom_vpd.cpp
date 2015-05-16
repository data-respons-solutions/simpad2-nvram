/*
 * MtdVpd.cpp
 *
 *  Created on: Apr 26, 2015
 *      Author: hcl
 */

#include "eeprom_vpd.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "crc32.h"
#include <string.h>

EepromVpd::EepromVpd(const std::string& file, int size, int wpGpio)
{
	_filePath = file;
	_wpGpio = wpGpio;
	_eeSize = size;
}

EepromVpd::~EepromVpd()
{
}

bool EepromVpd::load(std::list<std::string>& to)
{
	struct stat buf;
	int fd = open(_filePath.c_str(), O_RDONLY);

    if (fd < 0 )
    {
        std::cerr << __func__ << ": No MTD at " << _filePath << std::endl;
        return false;
    }

	uint8_t *image = new uint8_t[_eeSize];

	ssize_t readBytes = read(fd, image, _eeSize);
	if (readBytes <= 0)
	{
		std::cerr << __func__ << ": Unable to read eeprom\n";
		close(fd);
		return false;
	}
	close(fd);

	uint32_t crc = computeCRC(0, image+4, _eeSize-4);
	uint32_t crc_read = (image[3] << 24) | (image[2] << 16) | (image[1] << 8) | image[0];
	if (crc_read != crc)
	{
		std::cerr << "EepromVpd::load: CRC error\n";
		delete[] image;
		return false;
	}

	char *p = (char*)(image+4);
	int left = _eeSize-4;

	while (left > 0)
	{
		int n = strnlen(p, left);
		to.push_back(p);
		p += n+1;
		left = left - (n+1);
	}

	delete[] image;

	return true;
}

bool EepromVpd::store(const std::list<std::string>& strings)
{
	bool ok=true;
	int gpioFd = -1;
	char gpioCommand[4] = "0\n";
	int fd = open(_filePath.c_str(), O_RDWR);

    if (fd < 0 )
    {
        std::cerr << __func__ << ": No file at " << _filePath << std::endl;
        return false;
    }

    if (_wpGpio >= 0)
    {
    	std::string gpioPath = "/sys/class/gpio/gpio" + _wpGpio;
    	gpioPath.append("/value");
    	gpioFd = open(gpioPath.c_str(), O_WRONLY);
    	if (gpioFd < 0)
    	{
    		std::cerr << __func__ << ": Unable to open gpio " << gpioPath << std::endl;
    		close(fd);
    		return false;
    	}
    	write(gpioFd, gpioCommand, 2);
    }
    uint8_t *image = new uint8_t[_eeSize];
    char *p = (char*)image+4;
    std::list<std::string>::const_iterator it = strings.cbegin();
    while (it != strings.cend())
    {
    	strcpy(p, it->c_str());
    	p += it->size() + 1;
    	it++;
    }

    uint32_t crc = computeCRC(0, image+4, _eeSize-4);
    image[3] = (crc >> 24) & 0xff;
    image[2] = (crc >> 16) & 0xff;
    image[1] = (crc >> 8) & 0xff;
    image[0] = crc & 0xff;

    ssize_t written = write(fd, image, _eeSize);
    if (written <= 0)
    {
    	std::cerr << __func__ << ": Unable to write eeprom\n";
    	ok = false;
    }
    close(fd);
    if (gpioFd >= 0)
    {
    	gpioCommand[0] = '1';
    	write(gpioFd, gpioCommand, 2);
    	close(gpioFd);
    }
    return ok;
}
