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

struct nvram_interface_priv {
	char *path;
};

int nvram_interface_init(struct nvram_interface_priv** priv, const char* section_a, const char* section_b)
{
	(void) section_b;

	struct nvram_interface_priv *pbuf = malloc(sizeof(struct nvram_interface_priv));
	if (!pbuf) {
		return -ENOMEM;
	}
	pbuf->path = (char*) section_a;

	*priv = pbuf;

	return 0;
}

int nvram_interface_size(struct nvram_interface_priv* priv, enum nvram_section_name section, size_t* size)
{
	switch (section) {
	case NVRAM_SECTION_A:
		break;
	case NVRAM_SECTION_B:
	default:
		*size = 0;
		return 0;
	}

	const char* path = nvram_interface_path(priv, section);
	if (!path) {
		return -EINVAL;
	}

	struct stat sb;
	if (stat(path, &sb)) {
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

int nvram_interface_read(struct nvram_interface_priv* priv, enum nvram_section_name section, uint8_t* buf, size_t size)
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

int nvram_interface_write(struct nvram_interface_priv* priv, enum nvram_section_name section, const uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	const char* path = nvram_interface_path(priv, section);
	if (!path) {
		return -EINVAL;
	}

	uint8_t* pbuf = NULL;
	int r = 0;

	r = set_immutable(path, false);
	if (r) {
		return r;
	}

	int fd = open(path, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
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
	set_immutable(path, true);
	free(pbuf);
	close(fd);
	return r;
}

const char* nvram_interface_path(const struct nvram_interface_priv* priv, enum nvram_section_name section)
{
	switch (section) {
	case NVRAM_SECTION_A:
	case NVRAM_SECTION_B:
		return priv->path;
	default:
		return NULL;
	}
}
