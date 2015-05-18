#include <iostream>
#include <string.h>
#include <syslog.h>
#include "vpd.h"
#include "crc32.h"


VPD::VPD(bool legacyMode) :
    _modified(false),
	_legacyMode(legacyMode)
{
	if (legacyMode)
	{
		_immutables.push_back("ETH_MAC_ADDR");
		_immutables.push_back("HW_BOARD_REVISION");
		_immutables.push_back("LM_PRODUCT_DATE");
		_immutables.push_back("LM_PRODUCT_ID");
		_immutables.push_back("LM_PRODUC_SERIAL");
	}
}

VPD::~VPD()
{
}

bool VPD::load(VpdStorage *st, bool immutables)
{
	std::list<std::string> sList;
	if ( !st->load(sList))
	{
		syslog(LOG_WARNING, "VPD::load: unable to load file\n");
		return false;
	}

	std::list<std::string>::const_iterator it = sList.cbegin();
	while (it != sList.cend())
	{
		int pos = it->find_first_of('=');
		if ( pos == 0 || pos == std::string::npos)
			syslog(LOG_WARNING, "VPD::load: Malformed line %s\n", it->c_str());
		else
		{
			std::string key = it->substr(0, pos);
			if (pos+1 != std::string::npos)
			{
				std::string value = it->substr(pos+1);
				insert(key, value);
				if (immutables)
					_immutables.push_back(key);
			}
		}
		it++;
	}
	return true;
}

bool VPD::store(VpdStorage* st)
{
	std::list<std::string> sList;
	KeyMap::const_iterator it = _map.cbegin();
	while (it != _map.cend())
	{
		if (_legacyMode || (!_legacyMode && !keyImmutable(it->first)))
		{
			std::string s = it->first + "=" + it->second;
			sList.push_back(s);
		}
		it++;
	}
	return st->store(sList);
}

bool VPD::keyImmutable(const std::string& key)
{
	std::list<std::string>::const_iterator it = _immutables.cbegin();
	while (it != _immutables.cend())
	{
		if ( *it == key )
			return true;
		it++;
	}
	return false;
}

void VPD::init(VpdStorage *st)
{
	KeyMap::iterator it = _map.begin();
	while (it != _map.end())
	{
		if (!keyImmutable(it->first))
			it = _map.erase(it);
		else
			it++;
	}
	_modified = true;
	if (st)
		load(st);

}

bool VPD::keyOk(const std::string& key)
{
    if (key.empty())
        return false;
    return true;
}

bool VPD::insert(const std::string& key, const std::string& value)
{
    if (keyOk(key))
    {
    	KeyMap::const_iterator it = _map.find(key);
    	if (keyImmutable(key) && it != _map.cend())
    	{
    		std::cerr << "Immutable key " << key << std::endl;
    		return false;
    	}
    	if (it == _map.cend())
    		_map.insert(std::pair<std::string, std::string>(key,value));
    	else
    		_map[key] = value;
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

void VPD::keys()
{
	KeyMap::const_iterator it = _map.cbegin();
	while (it != _map.cend())
	{
		std::cout <<  it->first << std::endl;
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

bool VPD::deleteKey(const std::string& key)
{
    KeyMap::iterator it  = _map.find(key);
    if (keyImmutable(key))
    {
    	std::cerr << "Key [" << key << "] is immutable\n";
    	return false;
    }
    if ( it == _map.end())
        return false;
    _map.erase(it);
    _modified = true;
    return true;
}

