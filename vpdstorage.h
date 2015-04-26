#ifndef VPDSTORAGE_H
#define VPDSTORAGE_H

#include <stdint.h>

class VpdStorage
{
public:
    VpdStorage() {}
    virtual ~VpdStorage() {}
    virtual uint8_t* load(int& size) = 0;
    virtual bool store(const uint8_t *vpdData, int size) = 0;
};

#endif // VPDSTORAGE_H
