#include <string>
#include <iostream>
#include "filevpd.h"
#include "crc32.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

FileVpd::FileVpd(const std::string& filePath)
{
	_filePath = filePath;
	_vpdFile = 0;
	_image = NULL;
}

FileVpd::~FileVpd()
{
    if ( _vpdFile )
    	fclose(_vpdFile);
    if (_image)
    	delete[] _image;
}

uint8_t* FileVpd::load(int& size)
{
	struct stat buf;
	_vpdFile = fopen(_filePath.data(), "r");

    if (_vpdFile == NULL )
    {
        std::cerr << __func__ << ": No VPD file\n";
        return NULL;
    }
    if (fstat(fileno(_vpdFile), &buf))
    {
    	std::cerr << __func__ << ": Can not stat file\n";
    	return 0;
    }

	_image = new uint8_t[buf.st_size+4];

	int readBytes = fread(_image+4, 1, buf.st_size, _vpdFile);
	for (int n=4; n < buf.st_size+4; n++)
		if (_image[n] == '\n')
			_image[n] = '\0';
	uint32_t crc = computeCRC(0, _image+4, buf.st_size);

	_image[3] = (crc >> 24) & 0xff;
	_image[2] = (crc >> 16) & 0xff;
	_image[1] = (crc >> 8) & 0xff;
	_image[0] = (crc & 0xff);
	fclose(_vpdFile);
	_vpdFile = NULL;
	size = readBytes + 4;
	return _image;
}

bool FileVpd::store(const uint8_t *vpdData, int size)
{
	_vpdFile = fopen(_filePath.data(), "w");
	if (!_vpdFile)
		return false;

	uint8_t *p = new uint8_t[size];
	memcpy(p, vpdData+4, size-4);
    for (int n=0; n < size-4; n++)
    {
        if (p[n]== '\0')
            p[n]= '\n';
    }
    fwrite(p, 1, size-4, _vpdFile);
    delete[] p;
    fclose(_vpdFile);
    return true;
}
