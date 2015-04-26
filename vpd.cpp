#include <iostream>
#include <string.h>
#include "vpd.h"
#include "crc32.h"

VPD::VPD(uint8_t *image, int imageSize) :
    _modified(false)
{
    _imageOutSize = imageSize;
    _imageOut = new uint8_t[_cMaxSize];
    memcpy(_imageOut, image, imageSize);
    parseImage();
}

VPD::~VPD()
{
	delete[] _imageOut;
}

bool VPD::keyOk(const std::string& key)
{
    if (key.empty())
        return false;
    return true;
}

int VPD::parseImage()
{
    _map.clear();
    uint32_t computedCrc = computeCRC(0, _imageOut + 4, _imageOutSize-4);
    uint32_t crc = (_imageOut[3] << 24) | (_imageOut[2] << 16) | (_imageOut[1] << 8) | _imageOut[0];
    if (computedCrc != crc)
    {
        std::cerr << "No existing VPD or broken VPD";
        return 0;
    }
    int left = _imageOutSize - 4;
    char *s = (char*)_imageOut + 4;
    while ( left > 0 )
    {
        int n = strnlen(s, left);
        if ( n >= left || n==0 )
            break;
        left -= n+1;
        std::string str(s);
        int pos = str.find_first_of('=');
        if ( pos == 0 || pos == std::string::npos)
        	std::cerr << "Malformed line " << str;
        else
        {
        	std::string key = str.substr(0, pos);
        	if (pos+1 != std::string::npos)
        	{
        		std::string value = str.substr(pos+1);
        		_map.insert(std::pair<std::string, std::string>(key, value));
        	}
        }
        s += n+1;
    }
    return _map.size();
}

bool VPD::insert(const std::string& key, const std::string& value)
{
    if (keyOk(key))
    {
    	_map.insert(std::pair<std::string, std::string>(key, value));
        _modified = true;
        return true;
    }
    else
    {
        std::cerr << "Malformed key " << key << std::endl;
        return false;
    }
}

void VPD::list()
{
    KeyMap::const_iterator it = _map.cbegin();
    while (it != _map.cend())
    {
    	std::cout <<  it->first << "=" << it->second << std::endl;
        it++;
    }
}

bool VPD::lookup(const std::string& key, std::string &value)
{
    KeyMap::const_iterator it  = _map.find(key);
    if ( it == _map.cend())
        return false;

    value = it->second;
    return true;
}

bool VPD::insert(const std::string& key, std::string& value)
{
    if (keyOk(key))
    {
        _map.insert(std::pair<std::string, std::string>(key, value));
        _modified = true;
        return true;
    }
    else
        return false;
}

bool VPD::prepareWrite()
{
    _imageOutSize=0;
    int at=4;
    KeyMap::const_iterator it = _map.cbegin();
    while (it != _map.cend())
    {
        std::string entry(it->first);
        entry +=  "=" + it->second;
        memcpy(&_imageOut[at], entry.c_str(), entry.size()+1);
        it++;
        at += entry.size()+1;
    }
    _imageOutSize = at;
    uint32_t crc = computeCRC(0, _imageOut+4, _imageOutSize-4);
    _imageOut[0] = crc & 0xff;
    _imageOut[1] = (crc >> 8) & 0xff;
    _imageOut[2] = (crc >> 16) & 0xff;
    _imageOut[3] = (crc >> 24) & 0xff;
    return true;
}

uint8_t *VPD::getImage(int& size)
{

    if (_modified)
    {
        _modified = false;
        bool res = prepareWrite();
        size = _imageOutSize;
        if (res)
            return _imageOut;
        else
            return NULL;
    }
    size = _imageOutSize;
    return _imageOut;
}

bool VPD::deleteKey(const std::string& key)
{
    KeyMap::iterator it  = _map.find(key);
    if ( it == _map.end())
        return false;
    _map.erase(it);
    _modified = true;
    return true;
}

