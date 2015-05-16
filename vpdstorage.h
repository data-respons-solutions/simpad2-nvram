#ifndef VPDSTORAGE_H
#define VPDSTORAGE_H

#include <stdint.h>
#include <string>
#include <list>

class VpdStorage
{
public:
    VpdStorage() {}
    virtual ~VpdStorage() {}
    virtual bool load(std::list<std::string>& to) = 0;
    virtual bool store(const std::list<std::string>& l) = 0;
};

#endif // VPDSTORAGE_H
