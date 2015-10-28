#include "filevpd.h"
#include "vpd.h"
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include "eeprom_vpd.h"
#include <sys/stat.h>

#define EEPROM_SIZE 0x800

static const std::string initFile = "/nvram/factory/vpd";

void usage(void)
{
	std::cout << "nvram, EEPROM tool (C) DATA RESPONS AS" << std::endl;
	std::cout << "Usage:  nvram COMMAND [KEY [VALUE]]" << std::endl;
	std::cout << "Commands:" << std::endl;
	std::cout << "\tset KEY VALUE\tAssign VALUE to KEY in nvram." << std::endl << "\t\t\tAdd entry if not exist" << std::endl;
	std::cout << "\tget KEY\t\tRead contents of KEY to stdout" << std::endl;
	std::cout << "\tdelete KEY\tDelete the entry given by KEY" << std::endl;
	std::cout << "\tkeys\t\tList all keys to stdout" << std::endl;
	std::cout << "\tlist\t\tList all keys and values to stdout" << std::endl;
	std::cout << "\tinit\t\tClears all but immutable keys" << std::endl;
}

int main(int argc, char *argv[])
{
	int ret=0;
	std::list<std::string> argvList;
	for (int n=1; n < argc; n++)
	{
		std::string str(argv[n]);
		argvList.push_back(str);
	}

	VpdStorage *factoryStorage=0;
	VpdStorage *userStorage=0;
#if defined(TARGET_LEGACY)
	VPD vpd(true);

	char *gpio = getenv("VPD_EEPROM_GPIO");
	int gpio_nr = gpio ? atoi(gpio) : -1;
	std::string eepromFullPath = "/home/hcl/legacy/eeprom";
	char *epath = getenv("VPD_EEPROM_PATH");
	if (epath)
		eepromFullPath = epath;

	struct stat buf;
	ret = stat(eepromFullPath.c_str(), &buf);
	if (ret == 0 && !S_ISREG(buf.st_mode))
	{
		std::cerr << "Eeprom file path " << eepromFullPath << " invalid -exit\n";
		return -1;
	}
#ifdef DEBUG
	std::cout << "Eeprom at " << eepromFullPath << std::endl;
#endif
	userStorage = new EepromVpd(eepromFullPath, EEPROM_SIZE, gpio_nr);
	if (!vpd.load(userStorage))
	{
		std::cerr << "nvram: unable to load data from " << eepromFullPath << std::endl;
		delete userStorage;
		return -1;
	}
#else
	VPD vpd;
	const char *vpdPathFactory = getenv("VPD_FACTORY");
	if (!vpdPathFactory)
		vpdPathFactory = "/nvram/factory/vpd";

	struct stat buf;
	ret = stat(vpdPathFactory, &buf);
	if (ret == 0 && !S_ISREG(buf.st_mode))
	{
		std::cerr << "Factory file path " << vpdPathFactory << " invalid - exiting \n";
		return -1;
	}

	if (vpdPathFactory)
	{
		factoryStorage = new FileVpd(vpdPathFactory);
		if (!vpd.load(factoryStorage, true))
		{
			std::cerr <<  "No factory VPD data in " << vpdPathFactory << " exiting \n";
			delete factoryStorage;
			return -1;
		}
	}

	const char *vpdPathUser = getenv("VPD_USER");
	if (!vpdPathUser)
		vpdPathUser = "/nvram/user/vpd";

	ret = stat(vpdPathUser, &buf);
	if (ret == 0 && !S_ISREG(buf.st_mode))
	{
		std::cerr << "User file path " << vpdPathUser << " not a regular file";
		delete factoryStorage;
		return -1;
	}

    userStorage = new FileVpd(vpdPathUser);
    vpd.load(userStorage);
#endif

	ret = 0;

    if (argvList.empty() || argvList.front() == "list")
    {
    	vpd.list();
    	goto cleanexit;
    }

    if (argvList.front() == "get")
    {
    	if (argvList.size() < 2)
    	{
    		usage();
    		goto cleanexit;
    	}
    	std::string value;
    	argvList.pop_front();
    	if (vpd.lookup(argvList.front(), value))
    		std::cout << value << std::endl;
    	else
    		ret = -1;
    	goto cleanexit;
    }

    if (argvList.front() == "set")
    {
    	if (argvList.size() < 3)
    	{
    		usage();
    		ret = -1;
    		goto cleanexit;
    	}
    	argvList.pop_front();
    	std::string key = argvList.front();
    	argvList.pop_front();
    	if (!vpd.insert(key, argvList.front()))
    	{
    		std::cerr << "Unable to insert key [" << key << "]\n";
    		ret = -1;
    	}
    	goto cleanexit;
    }

    if (argvList.front() == "delete")
    {
    	if (argvList.size() < 2)
    	{
    		usage();
    		ret = -1;
    		goto cleanexit;
    	}
    	argvList.pop_front();
    	if (!vpd.deleteKey(argvList.front()))
    	{
    		std::cerr << "Key [" << argvList.front() << "] could not be erased\n";
    		ret = -1;
    	}
    	goto cleanexit;
    }

    if (argvList.front() == "keys")
	{
    	vpd.keys();
    	goto cleanexit;
	}

    if (argvList.front() == "init")
	{
    	vpd.init();
		goto cleanexit;
	}

    usage();
    ret = -1;

cleanexit:
    if (vpd.isModified())
        vpd.store(userStorage);
    if (factoryStorage)
    	delete factoryStorage;
    if (userStorage)
    	delete userStorage;
    return ret;
}

