#include "common.h"
#include "filevpd.h"
#include "vpd.h"
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <list>

#ifdef TARGET_DESKTOP
#define VPD_SIZE 0x800
#endif


void usage(void)
{
	std::cout << "nvram, EEPROM tool (C) DATA RESPONS AS" << std::endl;
	std::cout << "Usage:  nvram [OPTION] COMMAND [KEY [VALUE]]" << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "\t--help, -h\tDisplay help" << std::endl;
	std::cout << "\t--quiet, -q\tQuiet output" << std::endl;
	std::cout << "Commands:" << std::endl;
	std::cout << "\tset KEY VALUE\tAssign VALUE to KEY in nvram." << std::endl << "\t\t\tAdd entry if not exist" << std::endl;
	std::cout << "\tget KEY\t\tRead contents of KEY to stdout" << std::endl;
	std::cout << "\tdelete KEY\tDelete the entry given by KEY" << std::endl;
	std::cout << "\tkeys\t\tList all keys to stdout" << std::endl;

	std::cout << "\tlist\t\tList all keys and values to stdout" << std::endl;
	std::cout << "\tinit\t\tInitialise config. Config will be empty afterwards." << std::endl;
}

int main(int argc, char *argv[])
{
	std::list<std::string> argvList;
	for (int n=1; n < argc; n++)
	{
		std::string str(argv[n]);
		argvList.push_back(str);
	}
#if TARGET_TYPE == DESKTOP
	char *vpdPath = getenv("VPDFILE");
	std::string vpdString = vpdPath ? vpdPath : "/var/lib/vpddata";
    VpdStorage *vpdStorage = new FileVpd(vpdString);
#else
#if TARGET_TYPE == GEN2
    VpdStorage *vpdStorage = new MtdVpd("/dev/mtd2", 0x10000);
#endif
#endif


    int imageSize;
    uint8_t *image = vpdStorage->load(imageSize);
    if (image == 0)
    {
        std::cout << "No VPD object\n";
        delete vpdStorage;
        return -1;
    }
    VPD vpd(image, imageSize);

    if (argvList.empty() || argvList.front() == "list")
    {
    	vpd.list();
    	return 0;
    }

    if (argvList.front() == "get")
    {
    	if (argvList.size() < 2)
    	{
    		usage();
    		return -1;
    	}
    	std::string value;
    	argvList.pop_front();
    	if (vpd.lookup(argvList.front(), value))
    	{
    		std::cout << value << std::endl;
    		return 0;
    	}
    	std::cerr << "KEY [" << argvList.front() << "] not found\n";
    	return -1;
    }

    if (argvList.front() == "set")
    {
    	if (argvList.size() < 3)
    	{
    		usage();
    		return -1;
    	}
    	argvList.pop_front();
    	std::string key = argvList.front();
    	argvList.pop_front();
    	if (!vpd.insert(key, argvList.front()))
    	{
    		std::cerr << "Unable to insert key [" << key << "]\n";
    		return -1;
    	}
    	goto cleanexit;
    }

    if (argvList.front() == "delete")
    {
    	if (argvList.size() < 2)
    	{
    		usage();
    		return -1;
    	}
    	argvList.pop_front();
    	if (!vpd.deleteKey(argvList.front()))
    	{
    		std::cerr << "No key [" << argvList.front() << "] found\n";
    		return -1;
    	}
    	else
    		goto cleanexit;
    }

    usage();
    return -1;

cleanexit:
    if (vpd.isModified())
    {
    	int size;
        uint8_t *modImage = vpd.getImage(size);
        vpdStorage->store(modImage, size);
    }
    return 0;
}

