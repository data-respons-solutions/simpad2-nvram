#ifndef EFIVPD_H
#define EFIVPD_H
#include <vector>
#include "vpdstorage.h"
#include <cstdint>

struct EfiGuid{
	uint32_t	Data1;
    uint16_t	Data2;
    uint16_t	Data3;
    uint8_t		Data4[8];
};

class EfiVpd : public VpdStorage
{
public:
	EfiVpd(const std::string& efivarfs_path, const std::string& name, const EfiGuid& guid);
    ~EfiVpd() {}
    bool load(std::list<std::string>& to);
    bool store(const std::list<std::string>& strings);
private:
    std::string _filePath;
    std::vector<char> _buf;
    const uint32_t efiattr {0x7};
    const int cmax {1024};
    const int efimax {30*cmax};
};

#endif // EFIVPD_H
