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
	_buf = new char[cMax];
}

FileVpd::~FileVpd()
{
	delete _buf;
}

bool FileVpd::load(std::list<std::string>& sList)
{

	struct stat status;
	if (stat(_filePath.c_str(), &status) < 0)
		return true;
	std::ifstream is(_filePath);
	if (!is.is_open())
	{
		return false;
	}

	while (1)
	{
		is.getline(_buf, cMax);
		if ( is.eof())
			break;

		if (is.fail())
		{
			std::cerr << "VPD file contains string longer than max " << cMax << "\n";
			break;
		}

		int length = strnlen(_buf, cMax);
		if (length > 0 && _buf[length-1] == '\n')
			_buf[length-1]= '\0';
		sList.push_back(_buf);
	}
	return true;
}

bool FileVpd::store(const std::list<std::string>& strings)
{
	std::ofstream os(_filePath);
	if (!os.is_open())
	{
		return false;
	}

	std::list<std::string>::const_iterator it = strings.cbegin();
	while (it != strings.cend())
		os << (*it++) << std::endl;
	os.flush();
	os.close();
	return true;

}
