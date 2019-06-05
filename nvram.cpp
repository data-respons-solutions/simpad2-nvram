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
#include <mtd/libmtd.h>
const char *vpd_mtd_label = XSTR(VPD_MTD_LABEL);

static int find_mtd(const char* label, int* mtd_num, long long* mtd_size)
{
	libmtd_t mtd = nullptr;
	struct mtd_dev_info *mtd_dev = nullptr;
	struct mtd_info *mtd_info = nullptr;
	int status = -1;

	mtd = libmtd_open();
		if (!mtd) {
			switch (errno) {
			case 0:
				std::cerr<< "mtd subsystem not available" << " - exiting\n";
				goto errorexit;
			default:
				std::cerr<< "libmtd_open:  " << std::strerror(errno)  << " - exiting\n";
				goto errorexit;
			}
		}

		mtd_info = (struct mtd_info*) malloc(sizeof(struct mtd_info));
		if (mtd_get_info(mtd, mtd_info)) {
			std::cerr<< "mtd_get_info " << std::strerror(errno)  << " - exiting\n";
			goto errorexit;

		}

		mtd_dev = (struct mtd_dev_info*) malloc(sizeof(struct mtd_dev_info));
		for (int i = mtd_info->lowest_mtd_num; i <= mtd_info->highest_mtd_num; ++i) {
			if (mtd_get_dev_info1(mtd, i, mtd_dev)) {
				std::cerr<< "mtd_get_dev_info(node: " << i << "): " << std::strerror(errno)  << " - exiting\n";
				goto errorexit;
			}

			if (!strcmp(mtd_dev->name, label)) {
				*mtd_num = mtd_dev->mtd_num;
				*mtd_size = mtd_dev->size;
				break;
			}

			if (i == mtd_info->highest_mtd_num) {
				std::cerr << "find_mtd: device with label: \"" << label << "\": not found - exiting\n";
				goto errorexit;
			}
		}

	status = 0;

errorexit:
	if (mtd) {
		libmtd_close(mtd);
		mtd = nullptr;
	}
	if (mtd_dev) {
		free(mtd_dev);
		mtd_dev = nullptr;
	}

	if (mtd_info) {
		free(mtd_info);
		mtd_info = nullptr;
	}
	return status;
}
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
	VPD vpd(true);

	std::string mtd_label = vpd_mtd_label;
	char *elabel = getenv("VPD_MTD_LABEL");
		if (elabel)
			mtd_label = elabel;

	int mtd_num = -1;
	long long mtd_size = -1;
	if (find_mtd(mtd_label.c_str(), &mtd_num, &mtd_size)) {
		return -1;
	}

	struct stat buf;
	std::string mtd_path = "/dev/mtd" + std::to_string(mtd_num);
	ret = stat(mtd_path.c_str(), &buf);
	if (ret == 0 && !S_ISCHR(buf.st_mode))
	{
		std::cerr << "mtd file path invalid: " << mtd_path << " - exiting\n";
		return -1;
	}

	userStorage = new EepromVpdNoFS(mtd_path, mtd_size);
	if (!vpd.load(userStorage))
	{
		std::cerr << "nvram: unable to load data from " << mtd_path << std::endl;
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

