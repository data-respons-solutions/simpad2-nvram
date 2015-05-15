#include "common.h"
#include "filevpd.h"
#include "vpd.h"
#include "MtdVpd.h"
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
	std::list<std::string> argvList;
	for (int n=1; n < argc; n++)
	{
		std::string str(argv[n]);
		argvList.push_back(str);
	}

	VpdStorage *vpdStorage;
#if defined(TARGET_DESKTOP) || defined(TARGET_LMPLUS)
	char *vpdPath = getenv("VPDFILE");
	std::string vpdString = vpdPath ? vpdPath : "/var/lib/vpddata";
    vpdStorage = new FileVpd(vpdString);
#endif


    int imageSize;
    uint8_t *image = vpdStorage->load(imageSize);
    VPD vpd(image, imageSize);

    if (image && (argvList.empty() || argvList.front() == "list"))
    {
    	vpd.list();
    	return 0;
    }

    if (image && argvList.front() == "get")
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

    if (image && argvList.front() == "delete")
    {
    	if (argvList.size() < 2)
    	{
    		usage();
    		return -1;
    	}
    	argvList.pop_front();
    	if (!vpd.deleteKey(argvList.front()))
    	{
    		std::cerr << "Key [" << argvList.front() << "] could not be erased\n";
    		return -1;
    	}
    	else
    		goto cleanexit;
    }

    if (image && argvList.front() == "keys")
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

