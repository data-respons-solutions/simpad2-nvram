#include <string>
#include <iostream>
#include "efivpd.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

const std::string guid_to_str(const EfiGuid& guid)
{
	const size_t guid_len = 36 + 1;
	char str[guid_len];

	snprintf(str, guid_len, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x%02x%02x",
			guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2],
			guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return std::string(str);
}

static std::string getFullPath(const std::string& efi_path, const std::string& name, const EfiGuid& guid)
{

	return std::string(efi_path + "/" + name + "-" + guid_to_str(guid));
}

EfiVpd::EfiVpd(const std::string& efivarfs_path, const std::string& name, const EfiGuid& guid)
{
	_filePath = getFullPath(efivarfs_path, name, guid);
	_buf.resize(efimax);
}

bool EfiVpd::load(std::list<std::string>& sList)
{
	struct stat status;
	if (stat(_filePath.c_str(), &status) < 0)
		return true;

	int fd = 0;
	fd = open(_filePath.c_str(), O_RDONLY);
	if (fd < 0) {
		return false;
	}

	int size = read(fd, _buf.data(), _buf.size());
	close(fd);
	if (size < 4) {
		std::cerr << "Failed reading from:  " << _filePath << ": " << std::strerror(errno) << "\n";
		return false;
	}

	std::vector<char>::iterator start = _buf.begin() + sizeof(efiattr);
	std::vector<char>::iterator stop;
	const std::vector<char>::iterator end = _buf.begin() + size;
	while ((stop = std::find(start, end, '\0')) != end) {
		sList.push_back(std::string(start, stop));
		start = stop+1;
	}

	return true;
}

bool EfiVpd::store(const std::list<std::string>& strings)
{
	memcpy(_buf.data(), reinterpret_cast<const char*>(&efiattr), sizeof(efiattr));

	int total_size = sizeof(efiattr);
	for (std::string s : strings) {
		int size = strnlen(s.c_str(), cmax);
		if (total_size + size + 1 > efimax) {
			std::cerr << "VPD entries larger than efivariable max " << efimax << "\n";
		}

		if (size == cmax) {
			memcpy(_buf.data() + total_size, s.c_str(), size);
			total_size += size;
		}
		else {
			memcpy(_buf.data() + total_size, s.c_str(), size + 1);
			total_size += size + 1;
		}
	}

	int fd = 0;
	fd = open(_filePath.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		std::cerr << "Failed opening:  " << _filePath << ": " << std::strerror(errno) << "\n";
		return false;
	}

	int ret = write(fd, _buf.data(), total_size);
	close(fd);
	if (ret != total_size) {
		std::cerr << "Failed writing to:  " << _filePath << ": " << std::strerror(errno) << "\n";
		return false;
	}

	return true;
}
