#include "filevpd.h"
#include "efivpd.h"
#include "vpd.h"
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <list>
#include "eeprom_vpd.h"
#include "eeprom_vpd_nofs.h"
#include <sys/stat.h>

//#define DEBUG

#define XSTR(a) STRINGIFY(a)
#define STRINGIFY(x) #x

#ifdef TARGET_MTD
#include "mtdvpd.h"
const char *vpd_mtd_label_a = XSTR(VPD_MTD_LABEL_A);
const char *vpd_mtd_label_b = XSTR(VPD_MTD_LABEL_B);
const char *vpd_mtd_wp = XSTR(VPD_MTD_WP);
#endif

#ifdef TARGET_LEGACY
const char *vpd_eeprom_path = XSTR(VPD_EEPROM_PATH);
#define EEPROM_SIZE 0x800
#endif

static const std::string initFile = "/nvram/factory/vpd";

#ifdef TARGET_EFI
static const std::string efivarfs_path = "/sys/firmware/efi/efivars";
static const std::string efi_user_var = "user";
static const EfiGuid efi_datarespons_guid = {0x604dafe4,0x587a,0x47f6,{0x86,0x04,0x3d,0x33,0xeb,0x83,0xda,0x3d}};
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
	int ret=0;
	std::list<std::string> argvList;
	for (int n=1; n < argc; n++)
	{
		std::string str(argv[n]);
		argvList.push_back(str);
	}

	VpdStorage *factoryStorage = NULL;
	VpdStorage *userStorage = NULL;
#if defined(TARGET_LEGACY)
	VPD vpd(true);

	std::string eepromFullPath = vpd_eeprom_path;
	if (eepromFullPath == "")
	{
		char *epath = getenv("VPD_EEPROM_PATH");
		if (epath)
			eepromFullPath = epath;
	}
	if (eepromFullPath == "")
	{
		std::cerr << "No path to eeprom given\n";
		return -1;
	}

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
	userStorage = new EepromVpd(eepromFullPath, EEPROM_SIZE);
	if (!vpd.load(userStorage))
	{
		std::cerr << "nvram: unable to load data from " << eepromFullPath << std::endl;
		delete userStorage;
		return -1;
	}
#elif defined(TARGET_MTD)
	VPD vpd;

	std::string mtd_label_a = vpd_mtd_label_a;
	char *elabel_a = getenv("VPD_MTD_LABEL_A");
		if (elabel_a)
			mtd_label_a = elabel_a;

	std::string mtd_label_b = vpd_mtd_label_b;
	char *elabel_b = getenv("VPD_MTD_LABEL_B");
		if (elabel_b)
			mtd_label_b = elabel_b;

	int mtd_num_a = -1;
	long long mtd_size_a = -1;
	if (find_mtd(mtd_label_a.c_str(), &mtd_num_a, &mtd_size_a)) {
		return -1;
	}

	int mtd_num_b = -1;
	long long mtd_size_b = -1;
	if (find_mtd(mtd_label_b.c_str(), &mtd_num_b, &mtd_size_b)) {
		return -1;
	}

	struct stat buf;
	std::string mtd_path_a = "/dev/mtd" + std::to_string(mtd_num_a);
	ret = stat(mtd_path_a.c_str(), &buf);
	if (ret == 0 && !S_ISCHR(buf.st_mode))
	{
		std::cerr << "mtd file path invalid: " << mtd_path_a << " - exiting\n";
		return -1;
	}

	std::string mtd_path_b = "/dev/mtd" + std::to_string(mtd_num_b);
	ret = stat(mtd_path_b.c_str(), &buf);
	if (ret == 0 && !S_ISCHR(buf.st_mode))
	{
		std::cerr << "mtd file path invalid: " << mtd_path_b << " - exiting\n";
		return -1;
	}

	std::string mtd_gpio = vpd_mtd_wp;
	char *egpio = getenv("VPD_MTD_WP");
		if (egpio)
			mtd_gpio = egpio;

	MtdVpd* userMtdStorage = new MtdVpd(mtd_path_a, mtd_size_a , mtd_path_b, mtd_size_b);
	userMtdStorage->set_wp_gpio(mtd_gpio);
	userStorage = userMtdStorage;
	if (!vpd.load(userStorage))
	{
		std::cerr << "nvram: unable to load data" << std::endl;
		delete userStorage;
		return -1;
	}
#elif defined(TARGET_EFI)
	VPD vpd;

	struct stat buf;
	ret = stat(efivarfs_path.c_str(), &buf);
	if (ret != 0 || !S_ISDIR(buf.st_mode))
	{
		std::cerr << "efivarfs not found on " << efivarfs_path << " - exiting\n";
		return -1;
	}

	userStorage = new EfiVpd(efivarfs_path, efi_user_var, efi_datarespons_guid);
	vpd.load(userStorage);
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

