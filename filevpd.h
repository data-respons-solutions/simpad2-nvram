#ifndef FILEVPD_H
#define FILEVPD_H
#include <fstream>
#include "vpdstorage.h"
#include <stdio.h>

class FileVpd : public VpdStorage
{
public:
    FileVpd(const std::string& filePath);
    ~FileVpd();
    uint8_t* load(int& size);
    bool store(const uint8_t *vpdData, int size);
private:
    FILE* _vpdFile;
    std::string _filePath;
    uint8_t *_image;
};

#endif // FILEVPD_H
