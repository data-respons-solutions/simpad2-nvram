#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include <mtd/libmtd.h>
#include <errno.h>
#include "nvram_interface.h"
#include "log.h"

#define xstr(a) str(a)
#define str(a) #a

#define NVRAM_ENV_WP_GPIO "NVRAM_WP_GPIO"
#ifdef NVRAM_WP_GPIO
static char* DEFAULT_NVRAM_WP_GPIO = xstr(NVRAM_WP_GPIO);
#endif

struct nvram_mtd {
	char *path;
	long long size;
};

struct nvram_interface_priv {
	struct nvram_mtd a_dev;
	struct nvram_mtd b_dev;
	char* gpio;
};

static int find_mtd(const char* label,  int* mtd_num, long long* mtd_size)
{
	int r = 0;
	libmtd_t mtd = NULL;
	struct mtd_dev_info *mtd_dev = (struct mtd_dev_info*) malloc(sizeof(struct mtd_dev_info));
	struct mtd_info *mtd_info = (struct mtd_info*) malloc(sizeof(struct mtd_info));
	if (!mtd_dev || !mtd_info) {
		r = -ENOMEM;
		goto exit;
	}

	mtd = libmtd_open();
	if (!mtd) {
		r = -errno;
		goto exit;
	}

	if (mtd_get_info(mtd, mtd_info)) {
		r = -errno;
		goto exit;
	}

	for (int i = mtd_info->lowest_mtd_num; i <= mtd_info->highest_mtd_num; i++) {
		if (mtd_get_dev_info1(mtd, i, mtd_dev)) {
			r = -errno;
			goto exit;
		}

		if (!strcmp(mtd_dev->name, label)) {
			*mtd_num = mtd_dev->mtd_num;
			*mtd_size = mtd_dev->size;
			break;
		}

		if (i == mtd_info->highest_mtd_num) {
			r = -ENODEV;
			goto exit;
		}
	}

	r = 0;

exit:
	if (mtd) {
		libmtd_close(mtd);
	}
	if (mtd_dev) {
		free(mtd_dev);
	}
	if (mtd_info) {
		free(mtd_info);
	}
	return r;
}

static int init_nvram_mtd(struct nvram_mtd* nvram_mtd, const char* label)
{
	const char *pathfmt = "/dev/mtd%d";
	long long mtd_size = 0LL;
	int mtd_num = 0;
	int r = 0;

	r = find_mtd(label, &mtd_num, &mtd_size);
	if (r) {
		return r;
	}
	pr_dbg("%s: found label \"%s\" with index: %d\n", __func__, label, mtd_num);

	r = snprintf(NULL, 0, pathfmt, mtd_num);
	if (r < 0) {
		return -EINVAL;
	}
	int bufsize = r + 1;

	nvram_mtd->path = (char*) malloc(bufsize);
	if (!nvram_mtd->path) {
		return -ENOMEM;
	}
	r = snprintf(nvram_mtd->path, bufsize, pathfmt, mtd_num);
	if (r != bufsize - 1) {
		free(nvram_mtd->path);
		nvram_mtd->path = NULL;
		return -EINVAL;
	}

	nvram_mtd->size = mtd_size;
	return 0;
}

int nvram_interface_init(struct nvram_interface_priv** priv, const char* section_a, const char* section_b)
{
	int r = 0;
	struct nvram_interface_priv *pbuf = malloc(sizeof(struct nvram_interface_priv));
	if (!pbuf) {
		return -ENOMEM;
	}
	memset(pbuf, 0, sizeof(struct nvram_interface_priv));

	r = init_nvram_mtd(&pbuf->a_dev, section_a);
	if (r) {
		free(pbuf);
		return r;
	}

	r = init_nvram_mtd(&pbuf->b_dev, section_b);
	if (r) {
		free(pbuf->a_dev.path);
		free(pbuf);
		return r;
	}

	pbuf->gpio = getenv(NVRAM_ENV_WP_GPIO);
#ifdef NVRAM_WP_GPIO
	if (!pbuf->gpio) {
		pbuf->gpio = DEFAULT_NVRAM_WP_GPIO;
	}
#endif

	*priv = pbuf;

	return 0;
}

int nvram_interface_size(struct nvram_interface_priv* priv, enum nvram_section section, size_t* size)
{
	switch (section) {
	case NVRAM_SECTION_A:
		*size = priv->a_dev.size;
		break;
	case NVRAM_SECTION_B:
		*size = priv->b_dev.size;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int nvram_interface_read(struct nvram_interface_priv* priv, enum nvram_section section, uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	const char* path = nvram_interface_path(priv, section);
	if (!path) {
		return -EINVAL;
	}

	int r = 0;
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		return -errno;
	}

	ssize_t bytes = read(fd, buf, size);
	if (bytes < 0) {
		r = -errno;
		goto exit;
	}
	else
	if ((size_t) bytes != size) {
		r = -EIO;
		goto exit;
	}

	r = 0;

exit:
	close(fd);
	return r;
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

static int set_gpio(const char* path, bool value)
{
	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		return -errno;
	}

	int r = write(fd, value ? "1" : "0", 1);
	close(fd);
	if (r == 1) {
		return 0;
	}
	return -errno;
}

int nvram_interface_write(struct nvram_interface_priv* priv, enum nvram_section section, const uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	const char* path = nvram_interface_path(priv, section);
	if (!path) {
		return -EINVAL;
	}

	int r = 0;
	int fd = open(path, O_WRONLY);
	if (fd < 0) {
		return -errno;
	}

	long long mtd_size = 0LL;
	r = nvram_interface_size(priv, section, (size_t*) &mtd_size);
	if (r) {
		goto exit;
	}

	if (priv->gpio) {
		r = set_gpio(priv->gpio, false);
		if (r) {
			goto exit;
		}
	}

	r = erase_mtd(fd, mtd_size);
	if (r) {
		goto exit;
	}

	ssize_t bytes = write(fd, buf, size);
	if (bytes < 0) {
		r = -errno;
		goto exit;
	}
	else
	if ((size_t) bytes != size) {
		r = -EIO;
		goto exit;
	}

	r = 0;

exit:
	if (priv->gpio) {
		set_gpio(priv->gpio, true);
	}

	close(fd);
	return r;
}

const char* nvram_interface_path(const struct nvram_interface_priv* priv, enum nvram_section section)
{
	switch (section) {
	case NVRAM_SECTION_A:
		return priv->a_dev.path;
	case NVRAM_SECTION_B:
		return priv->b_dev.path;
	default:
		return NULL;
	}
}
