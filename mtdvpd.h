/*
 * mtdvpd.h
 *
 *  Created on: Oct 28, 2019
 *      Author: ms
 */

#ifndef _MTDVPD_H_
#define _MTDVPD_H_
#include <string>
#include <stdint.h>
#include "vpdstorage.h"

int find_mtd(const char* label, int* mtd_num, long long* mtd_size);

class MtdVpd : public VpdStorage
{
public:
	MtdVpd(const std::string& dev_a, long long size_a, const std::string& dev_b, long long size_b);
	virtual ~MtdVpd();

	void set_wp_gpio(const std::string& wp_gpio_path);

	bool load(std::list<std::string>& to);
	bool store(const std::list<std::string>& strings);
private:
	std::string _section_a;
	long long _size_a;
	std::string _section_b;
	long long _size_b;
	std::string _wp_gpio_path; 	// Optional write protect (low) GPIO number
	int _active_section;
	uint32_t _active_section_counter;
};

#endif /* _MTDVPD_H_ */
