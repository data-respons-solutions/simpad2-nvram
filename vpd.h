#ifndef VPD_H
#define VPD_H
#include <map>
#include <stdint.h>
#include <string>
#include <list>
#include "vpdstorage.h"

typedef std::map<std::string, std::string> KeyMap;


class VPD
{
public:
    explicit VPD(bool legacyMode=false);
    ~VPD();
    bool isModified() { return _modified; }

public:
    void list();
    void keys();
    bool lookup(const std::string& key, std::string& value);
    bool insert(const std::string& key, const std::string& value);
    bool deleteKey(const std::string& key);
    uint8_t *getImage(int& size);
    void init(VpdStorage *st=0);
    bool load(VpdStorage *st, bool immutables=false);
    bool store(VpdStorage *st);
private:
    bool keyOk(const std::string& key);
    bool keyImmutable(const std::string& key);

    KeyMap _map;
    bool _modified;
    bool _legacyMode;
    std::list<std::string> _immutables;
};

#endif // VPD_H
