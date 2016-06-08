/*
 * MtdVpd.cpp
 *
 *  Created on: Jun 8, 2016
 *      Author: asi
 */

#include "eeprom_vpd_nofs.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "crc32.h"
#include <string.h>
#include <iomanip>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>

EepromVpdNoFS::EepromVpdNoFS(const std::string& file, int size, int wpGpio)
{
	_filePath = file;
	_wpGpio = wpGpio;
	_eeSize = size;
}

EepromVpdNoFS::~EepromVpdNoFS()
{
}

/*
 * First 2 Bytes is the size of total messages length held in eeprom => 0xffff (65535)
 * Next 4 bytes is checksum of that messages
 * from 7th byte messages start
 */

bool EepromVpdNoFS::load(std::list<std::string>& to)
{
	struct stat buf;
	int fd = open(_filePath.c_str(), O_RDONLY);

    if (fd < 0 )
    {
        std::cerr << __func__ << ": No device " << _filePath << std::endl;
        return false;
    }

	uint8_t *msg_len = new uint8_t[EEPROM_MSG_LEN];

	// read first 2 bytes to get crc + data length
	ssize_t msgLenReadBytes = read(fd, msg_len, EEPROM_MSG_LEN);
	if (msgLenReadBytes <= 0)
	{
		std::cerr << __func__ << ": Unable to read eeprom\n";
		close(fd);
		return false;
	}

	uint16_t eepromSize = (msg_len[1] << 8) | msg_len[0];

	if(eepromSize == EEPROM_EMPTY)
	{
		//std::cout << "EEPROM EMPTY" << std::endl;
		return true;
	}

	eepromSize += 4;	// already read 2 bytes of message length, 4 bytes of CRC checksum need to be read before msg

	uint8_t *image = new uint8_t[eepromSize];

	// read crc + data
	ssize_t readBytes = read(fd, image, eepromSize);
	if (readBytes <= 0)
	{
		std::cerr << __func__ << ": Unable to read eeprom\n";
		close(fd);
		return false;
	}

	close(fd);

	uint32_t crc = computeCRC(0, image+4, eepromSize-4);
	uint32_t crc_read = (image[3] << 24) | (image[2] << 16) | (image[1] << 8) | image[0];

	if (crc_read != crc)
	{
		std::cerr << "EepromVpdNoFS::load: CRC error\n";
		delete[] image;
		return false;
	}

	char *p = (char*)(image+4);
	int left = eepromSize-4;

	while (left > 0 && isascii(*p))
	{
		int n = strnlen(p, left);
		to.push_back(p);
		p += n+1;
		left = left - (n+1);
	}

	delete[] msg_len;
	delete[] image;

	return true;
}

bool EepromVpdNoFS::store(const std::list<std::string>& strings)
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

    uint16_t eepromSize = 0;
    for (std::list<std::string>::const_iterator it = strings.cbegin(); it != strings.cend(); it++)
    	eepromSize += (*it).length() + 1;

    eepromSize += 6; // 6 for data length 2 bytes & crc 4 bytes

    uint8_t *image = new uint8_t[eepromSize];
    memset(image, 0xff, eepromSize);
    char *p = (char*)image+6;
    std::list<std::string>::const_iterator it = strings.cbegin();
    while (it != strings.cend())
    {
    	strcpy(p, it->c_str());
    	p += it->size() + 1;
    	it++;
    }

    // Erase flash first
    erase_eeprom(fd, EEPROM_START_ADDR, EEPROM_MIN_ERASE_SIZE);

    // write data length
    image[1] = ((eepromSize - 6) >> 8) & 0xff;
    image[0] = (eepromSize - 6) & 0xff;

    uint32_t data_len = (image[1] << 8) | image[0];

    // write crc
    uint32_t crc = computeCRC(0, image+6, eepromSize-6);
    image[5] = (crc >> 24) & 0xff;
    image[4] = (crc >> 16) & 0xff;
    image[3] = (crc >> 8) & 0xff;
    image[2] = crc & 0xff;

    uint32_t crc_computed = (image[5] << 24) | (image[4] << 16) | (image[3] << 8) | image[2];

    ssize_t written = write(fd, image, eepromSize);
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

int EepromVpdNoFS::erase_eeprom(int fd, u_int32_t offset, u_int32_t bytes)
{
	int err;
	struct erase_info_user erase;
	erase.start = offset;
	erase.length = bytes;
	err = ioctl(fd, MEMERASE, &erase);
	if (err < 0) {
		perror("MEMERASE");
		return 1;
	}
	//fprintf(stderr, "Erased %d bytes from address 0x%.8x in flash\n", bytes, offset);
	return 0;
}
