/*
 * mtdvpd.cpp
 *
 *  Created on: Oct 28, 2019
 *      Author: ms
 */
#include <vector>
#include <iterator>
#include <iostream>
#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include <mtd/libmtd.h>
#include "libnvram/libnvram.h"
#include "mtdvpd.h"

#include <stdio.h>

enum mtd_section : int {
	UNKNOWN = 0,
	A,
	B,
};

/*
static void print_buf(const char* prefix, const std::vector<uint8_t>& buf)
{
	std::cout << prefix << ": buffer (" << buf.size() << "b):\n";
	for (uint8_t byte : buf) {
		printf(" 0x%02" PRIx8 "", byte);
	}
	std::cout << "\n";
}
*/

int find_mtd(const char* label, int* mtd_num, long long* mtd_size)
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

MtdVpd::MtdVpd(const std::string& dev_a, long long size_a, const std::string& dev_b, long long size_b)
{
	_section_a = dev_a;
	_size_a = size_a;
	_section_b = dev_b;
	_size_b = size_b;
	_active_section = mtd_section::UNKNOWN;
	_active_section_counter = 0;
}

MtdVpd::~MtdVpd()
{
}

void MtdVpd::set_wp_gpio(const std::string& wp_gpio_path)
{
	_wp_gpio_path = std::string("/sys/class/gpio/") + wp_gpio_path + "/value";
}

static int set_gpio(const std::string& gpio, bool value)
{
	int fd = open(gpio.c_str(), O_WRONLY);
	if (fd < 0) {
		std::cerr << "Failed opening " << gpio << " [" << errno << "]: " << std::strerror(errno) << std::endl;
		return -1;
	}

	int ret = write(fd, value ? "1" : "0", 1);
	close(fd);
	if (ret == 1) {
		return 0;
	}

	std::cerr << "Failed setting " << gpio << " to " << value << " [" << errno << "]: " << std::strerror(errno) << std::endl;
	return -1;
}

static int read_section(const std::string& path, long long size, std::vector<uint8_t>& buf)
{
	if (size < 0) {
		std::cerr << __func__ << ": invalid argument: size(" << size << ") < 0" << std::endl;
		return -1;
	}
	if (size > SIZE_MAX) {
		std::cerr << __func__ << ": invalid argument: size(" << size << ") > " << SIZE_MAX << std::endl;
		return -1;
	}

	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		std::cerr << path << ": open failed [" << errno << "]: " << std::strerror(errno) << std::endl;
		return -1;
	}

	buf.resize(size);
	int read_bytes = read(fd, buf.data(), static_cast<size_t>(size));
	if (read_bytes < 0) {
		std::cerr << path << ": failed reading [" << errno << "]: " << std::strerror(errno) << std::endl;
		goto error_exit;
	}
	else
	if (read_bytes != size) {
		std::cerr << path << ": read bytes(" << read_bytes << ") less than dev size(" << size << ")" << std::endl;
		goto error_exit;
	}

	close(fd);
	return 0;

error_exit:
	if (fd > 0) {
		close(fd);
	}
	return -1;
}

static int erase_mtd(int fd, long long size)
{
	if (size > UINT32_MAX || size < 0) {
		return -EINVAL;
	}

	struct erase_info_user erase_info;
	erase_info.start = 0;
	erase_info.length = size;
	int r = ioctl(fd, MEMERASE, &erase_info);
	if (r < 0) {
		return -errno;
	}
	return 0;
}

static int write_section(const std::string& path, long long size, std::vector<uint8_t>& buf)
{
	if (size < buf.size()) {
		std::cerr << path << ": data(" << buf.size() << ") larger than dev (" << size << ")" << std::endl;
		return -1;
	}

	int r = 0;

	int fd = open(path.c_str(), O_WRONLY);
	if (fd < 0) {
		std::cerr << path << ": open failed [" << errno << "]: " << std::strerror(errno) << std::endl;
		goto error_exit;
	}

	r = erase_mtd(fd, size);
	if (r) {
		std::cerr << path << ": failed erasing device [" << -r << "]: " << std::strerror(-r) << std::endl;
		goto error_exit;
	}

	r = write(fd, buf.data(), buf.size());
	if (r < 0) {
		std::cerr << path << ": failed writing [" << errno << "]: " << std::strerror(errno) << std::endl;
		goto error_exit;
	}
	else
	if (r != buf.size()) {
		std::cerr << path << ": wrote bytes(" << r << ") less than dev size(" << buf.size() << ")" << std::endl;
		goto error_exit;
	}

	close(fd);
	return 0;

error_exit:
	if (fd > 0) {
		close(fd);
	}
	return -1;
}

static int buf_to_list(const std::vector<uint8_t>& buf, uint32_t data_len, std::list<std::string>& to)
{
	struct nvram_list nvram_list = NVRAM_LIST_INITIALIZER;
	int ret = nvram_section_deserialize(&nvram_list, buf.data(), data_len);
	if (ret) {
		std::cerr << "failed deserializing data [" << -ret << "]: " <<  std::strerror(-ret) << std::endl;
		return -1;
	}

	struct nvram_node* cur = nvram_list.entry;
	while(cur) {
		to.push_back(std::string(cur->key) + "=" + cur->value);
		cur = cur->next;
	}

	destroy_nvram_list(&nvram_list);

	return 0;
}

static int list_to_buf(std::vector<uint8_t>& buf, uint32_t counter, const std::list<std::string>& from)
{
	int r = 0;
	struct nvram_list list = NVRAM_LIST_INITIALIZER;

	for (auto it = from.begin(); it != from.end(); it++) {
		size_t pos = it->find("=");
		std::string key(it->substr(0, pos));
		std::string value(it->substr(pos + 1, std::string::npos));
		nvram_list_set(&list, key.c_str(), value.c_str());
	}

	uint32_t size = 0;
	r = nvram_section_serialize_size(&list, &size);
	if (r < 0) {
		std::cerr << "failed calculating serialized buffer size [" << -r << "]: " << std::strerror(-r) << std::endl;
		goto error_exit;
	}
	buf.resize(size);
	r = nvram_section_serialize(&list, counter, buf.data(), buf.size());
	if (r < 0) {
		std::cerr << "failed serialized nvram_list [" << -r << "]: " << std::strerror(-r) << std::endl;
		goto error_exit;
	}

	destroy_nvram_list(&list);
	return 0;

error_exit:
	destroy_nvram_list(&list);
	return -1;
}

bool MtdVpd::load(std::list<std::string>& to)
{
	std::vector<uint8_t> buf_a;
	if (read_section(_section_a, _size_a, buf_a)) {
		return false;
	}

	std::vector<uint8_t> buf_b;
	if (read_section(_section_b, _size_b, buf_b)) {
		return false;
	}

	uint32_t counter_a = 0;
	uint32_t data_len_a = 0;
	int is_valid_a = is_valid_nvram_section(buf_a.data(), buf_a.size(), &data_len_a, &counter_a);
	uint32_t counter_b = 0;
	uint32_t data_len_b = 0;
	int is_valid_b = is_valid_nvram_section(buf_b.data(), buf_b.size(), &data_len_b, &counter_b);
	if (is_valid_a && (counter_a > counter_b)) {
		_active_section = mtd_section::A;
		_active_section_counter = counter_a;
		if (buf_to_list(buf_a, data_len_a, to)) {
			return false;
		}
	}
	else
	if (is_valid_b) {
		_active_section = mtd_section::B;
		_active_section_counter = counter_b;
		if (buf_to_list(buf_b, data_len_b, to)) {
			return false;
		}
	}
	else {
		// No active section found, expect fresh flash
		_active_section = mtd_section::B;
		_active_section_counter = 0;
	}

	return true;
}

bool MtdVpd::store(const std::list<std::string>& strings)
{
	std::vector<uint8_t> buf;
	list_to_buf(buf, _active_section_counter + 1, strings);

	if (!_wp_gpio_path.empty()) {
		if (set_gpio(_wp_gpio_path, false)) {
			return false;
		}
	}

	switch(_active_section) {
	case mtd_section::A:
		if (!write_section(_section_b, _size_b, buf)) {
			return false;
		}
		break;
	case mtd_section::B:
		if (!write_section(_section_a, _size_a, buf)) {
			return false;
		}
		break;
	default:
		std::cerr << "No active section defined" << std::endl;
		return false;
	}

	if (!_wp_gpio_path.empty()) {
		if (set_gpio(_wp_gpio_path, true)) {
			return false;
		}
	}

	return true;
}
