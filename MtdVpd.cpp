/*
 * MtdVpd.cpp
 *
 *  Created on: Apr 26, 2015
 *      Author: hcl
 */

#include "MtdVpd.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "crc32.h"

MtdVpd::MtdVpd(std::string& mtdPart, int mtdSize, int wpGpio)
{
	_filePath = mtdPart;
	_vpdFd = -1;
	_image = NULL;
	_wpGpio = wpGpio;
	_mtdSize = mtdSize;
}

MtdVpd::~MtdVpd()
{
    if ( _vpdFd > 0 )
    	close(_vpdFd);
    if (_image)
    	delete[] _image;
}

uint8_t* MtdVpd::load(int& size)
{
	struct stat buf;
	_vpdFd = open(_filePath.c_str(), O_RDONLY);

    if (_vpdFd < 0 )
    {
        std::cerr << __func__ << ": No MTD at " << _filePath << std::endl;
        return NULL;
    }

	_image = new uint8_t[_mtdSize];

	ssize_t readBytes = read(_vpdFd, _image, _mtdSize);
	if (readBytes <= 0)
	{
		std::cerr << __func__ << ": Unable to read MTD\n";
		return 0;
	}
	uint32_t crc = computeCRC(0, _image+4, _mtdSize-4);

	_image[3] = (crc >> 24) & 0xff;
	_image[2] = (crc >> 16) & 0xff;
	_image[1] = (crc >> 8) & 0xff;
	_image[0] = (crc & 0xff);
	close(_vpdFd);
	_vpdFd = -1;
	size = _mtdSize;
	return _image;
}

bool MtdVpd::store(const uint8_t *vpdData, int size)
{
	bool ok=true;
	int gpioFd = -1;
	char gpioCommand[4] = "0\n";
	char cmd[128];
	sprintf(cmd, "/usr/sbin/mtd_debug erase %s 0 %d", _filePath.c_str(), _mtdSize);
	system(cmd);
	_vpdFd = open(_filePath.c_str(), O_RDWR);

    if (_vpdFd < 0 )
    {
        std::cerr << __func__ << ": No MTD at " << _filePath << std::endl;
        return false;
    }
    if (_wpGpio >= 0)
    {
    	std::string gpioPath = "/sys/class/gpio/gpio/" + _wpGpio;
    	gpioFd = open(gpioPath.c_str(), O_WRONLY);
    	if (gpioFd < 0)
    	{
    		std::cerr << __func__ << ": Unable to open gpio " << gpioPath << std::endl;
    		close(_vpdFd);
    		_vpdFd = -1;
    		return false;
    	}
    	write(gpioFd, gpioCommand, 2);
    }


    ssize_t written = write(_vpdFd, vpdData, _mtdSize);
    if (written <= 0)
    {
    	std::cerr << __func__ << ": Unable to write MTD\n";
    	ok = false;
    }
    close(_vpdFd);
    if (gpioFd >= 0)
    {
    	gpioCommand[0] = '1';
    	write(gpioFd, gpioCommand, 2);
    	close(gpioFd);
    }
    return ok;
}
