#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "nvram_interface.h"

struct nvram_device {
	char *path;
};

int nvram_interface_init(struct nvram_device** dev, const char* section)
{
	if (!section || *dev) {
		return -EINVAL;
	}

	struct nvram_device *pbuf = malloc(sizeof(struct nvram_device));
	if (!pbuf) {
		return -ENOMEM;
	}
	pbuf->path = (char*) section;

	*dev = pbuf;

	return 0;
}

void nvram_interface_destroy(struct nvram_device** dev)
{
	if (*dev) {
		free(*dev);
		*dev = NULL;
	}
}

int nvram_interface_size(struct nvram_device* dev, size_t* size)
{
	struct stat sb;
	if (stat(dev->path, &sb)) {
		switch (errno) {
		case ENOENT:
			*size = 0;
			return 0;
		default:
			return -errno;
		}
	}

	*size = sb.st_size;
	return 0;
}

int nvram_interface_read(struct nvram_device* dev, uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	int fd = open(dev->path, O_RDONLY);
	if (fd < 0) {
		return -errno;
	}

	int r = 0;
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

exit:
	close(fd);
	return r;
}

int nvram_interface_write(struct nvram_device* dev, const uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	int fd = open(dev->path, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd < 0) {
		return -errno;
	}

	int r = 0;
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

exit:
	close(fd);
	return r;
}

const char* nvram_interface_section(const struct nvram_device* dev)
{
	return dev->path;
}
