#ifndef FILEVPD_H
#define FILEVPD_H
#include <fstream>
#include "vpdstorage.h"

class FileVpd : public VpdStorage
{
public:
    FileVpd(const std::string& filePath);
    ~FileVpd();
    bool load(std::list<std::string>& to);
    bool store(const std::list<std::string>& strings);
private:
    std::string _filePath;
};

#endif // FILEVPD_H
