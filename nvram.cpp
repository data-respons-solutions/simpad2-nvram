#include "common.h"
#include "filevpd.h"
#include "vpd.h"
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <list>
#include "eeprom_vpd.h"
#include <syslog.h>

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
	openlog("nvram", 0, LOG_USER);
	for (int n=1; n < argc; n++)
	{
		std::string str(argv[n]);
		argvList.push_back(str);
	}


	VpdStorage *vpdStorage;
#if defined(TARGET_LEGACY)
	VPD vpd(true);
	const std::string eepromInitialPath = "/sys/bus/i2c/devices";
	char *gpio = getenv("VPD_GPIO");
	int gpio_nr = gpio ? atoi(gpio) ? -1;
	vpdStorage = new EepromVpd(findEEprom(eepromInitialPath), EEPROM_SIZE, gpio_nr);
#else
	VPD vpd;
	const char *vpdPathFactory = getenv("VPD_FACTORY");
	if (!vpdPathFactory)
		vpdPathFactory = "/nvram/factory/vpd";



	if (vpdPathFactory)
	{
		VpdStorage *fact = new FileVpd(vpdPathFactory);
		if (!vpd.load(fact, true))
			syslog(LOG_WARNING, "No factory VPD data in %s\n", vpdPathFactory);
		delete fact;
	}

	const char *vpdPathUser = getenv("VPD_USER");
	if (!vpdPathUser)
		vpdPathUser = "/nvram/user/vpd";

    vpdStorage = new FileVpd(vpdPathUser);
#endif

    bool status = vpd.load(vpdStorage);
    if (!status)
    {
    	if (argvList.empty() || argvList.front() != "init")
    	{
    		syslog(LOG_INFO, "VPD User file %s will be initialized\n", vpdPathUser);
    		vpd.init();
    	}
    }

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
        vpd.store(vpdStorage);
    delete vpdStorage;
    closelog();
    return ret;
}

