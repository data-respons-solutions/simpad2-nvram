#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ext2fs/ext2_fs.h>
#include <e2p/e2p.h>
#include "nvram_interface.h"

struct efi_header {
	uint32_t attr;
};

static const struct efi_header EFI_HEADER = {0x7};

struct nvram_device {
	char *path;
};

int nvram_interface_init(struct nvram_device** dev, const char* section)
{
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

	if (sb.st_size < (off_t) sizeof(EFI_HEADER)) {
		return -EBADF;
	}

	*size = sb.st_size - sizeof(EFI_HEADER);
	return 0;
}

int nvram_interface_read(struct nvram_device* dev, uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	int r = 0;
	int fd = open(dev->path, O_RDONLY);
	if (fd < 0) {
		return -errno;
	}

	uint8_t* pbuf = (uint8_t*) malloc(size + sizeof(EFI_HEADER));
	if (!pbuf) {
		return -ENOMEM;
	}

	ssize_t bytes = read(fd, pbuf, size + sizeof(EFI_HEADER));
	if (bytes < 0) {
		r = -errno;
		goto exit;
	}
	else
	if ((size_t) bytes != size + sizeof(EFI_HEADER)) {
		r = -EIO;
		goto exit;
	}

	memcpy(buf, pbuf + sizeof(EFI_HEADER), size);

	r = 0;

exit:
	free(pbuf);
	close(fd);
	return r;
}

static int set_immutable(const char* path, bool value)
{
	unsigned long flags = 0LU;
	if (fgetflags(path, &flags) != 0) {
		return -errno;
	}

	if (value) {
		flags |= EXT2_IMMUTABLE_FL;
	}
	else {
		flags &= ~EXT2_IMMUTABLE_FL;
	}

	if (fsetflags(path, flags) != 0) {
		return -errno;
	}

	return 0;
}

int nvram_interface_write(struct nvram_device* dev, const uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	uint8_t* pbuf = NULL;
	int r = 0;

	r = set_immutable(dev->path, false);
	if (r) {
		return r;
	}

	int fd = open(dev->path, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd < 0) {
		r = -errno;
		goto exit;
	}

	pbuf = (uint8_t*) malloc(size + sizeof(EFI_HEADER));
	if (!pbuf) {
		r = -ENOMEM;
		goto exit;
	}

	memcpy(pbuf, &EFI_HEADER, sizeof(EFI_HEADER));
	memcpy(pbuf + sizeof(EFI_HEADER), buf, size);

	ssize_t bytes = write(fd, pbuf, size + sizeof(EFI_HEADER));
	if (bytes < 0) {
		r = -errno;
		goto exit;
	}
	else
	if ((size_t) bytes != size + sizeof(EFI_HEADER)) {
		r = -EIO;
		goto exit;
	}

	r = 0;

exit:
	set_immutable(dev->path, true);
	free(pbuf);
	close(fd);
	return r;
}

const char* nvram_interface_section(const struct nvram_device* dev)
{
	return dev->path;
}
