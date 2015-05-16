#include <string>
#include <iostream>
#include "filevpd.h"
#include "crc32.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fstream>

FileVpd::FileVpd(const std::string& filePath)
{
	_filePath = filePath;
}

FileVpd::~FileVpd()
{
}

bool FileVpd::load(std::list<std::string>& sList)
{
	char buf[256];

	std::ifstream is(_filePath);
	if (!is.is_open())
	{
		std::cerr << "FileVPD::load: Unable to open " << _filePath << std::endl;
		return false;
	}

	while (1)
	{
		is.getline(buf, 256);
		if ( is.eof())
			break;
		int length = strnlen(buf, 256);
		if (length > 0 && buf[length-1] == '\n')
			buf[length-1]= '\0';
		sList.push_back(buf);
	}
	return true;
}

bool FileVpd::store(const std::list<std::string>& strings)
{
	std::ofstream os(_filePath);
	if (!os.is_open())
	{
		std::cerr << "FileVPD::load: Unable to open " << _filePath << std::endl;
		return false;
	}

	std::list<std::string>::const_iterator it = strings.cbegin();
	while (it != strings.cend())
		os << (*it++) << std::endl;
	os.close();
}
