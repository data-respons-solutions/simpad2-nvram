#ifndef VPD_H
#define VPD_H
#include <map>
#include <stdint.h>
#include <string>

typedef std::map<std::string, std::string> KeyMap;

class VPD
{
public:
    explicit VPD(uint8_t *image, int imageSize);
    ~VPD();
    bool isModified() { return _modified; }

public:
    int parseImage();
    bool insert(const std::string& key, const std::string& value);
    void list();
    bool lookup(const std::string& key, std::string& value);
    bool insert(const std::string& key, std::string& value);
    bool deleteKey(const std::string& key);
    uint8_t *getImage(int& size);
private:
    bool keyOk(const std::string& key);
    bool prepareWrite();
    KeyMap _map;
    bool _modified;
    uint8_t *_imageOut;
    int _imageOutSize;
    const int _cMaxSize=0x100000;
};

#endif // VPD_H
