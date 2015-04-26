#include <string>
#include <iostream>
#include <dirent.h>

std::string findEEprom(const std::string& initalPath)
{
	DIR *dir;
	struct dirent *dirEntry;
    std::cout << "Search for EEPROM at " << initalPath;
    if ( (dir = opendir(initalPath.data())) != NULL )
	{

	}
    return std::string("");
}
