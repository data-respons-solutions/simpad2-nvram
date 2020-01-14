#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "nvram_interface.h"

struct nvram_interface_priv {
	char *a_path;
	char *b_path;
};

int nvram_interface_init(struct nvram_interface_priv** priv, const char* section_a, const char* section_b)
{
	struct nvram_interface_priv *pbuf = malloc(sizeof(struct nvram_interface_priv));
	if (!pbuf) {
		return -ENOMEM;
	}
	pbuf->a_path = (char*) section_a;
	pbuf->b_path = (char*) section_b;

	*priv = pbuf;

	return 0;
}

int nvram_interface_size(struct nvram_interface_priv* priv, enum nvram_section section, size_t* size)
{
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

	*size = sb.st_size;
	return 0;
}

int nvram_interface_read(struct nvram_interface_priv* priv, enum nvram_section section, uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	int r = 0;
	int fd = open(nvram_interface_path(priv, section), O_RDONLY);
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

exit:
	close(fd);
	return r;
}

int nvram_interface_write(struct nvram_interface_priv* priv, enum nvram_section section, const uint8_t* buf, size_t size)
{
	if (!buf) {
		return -EINVAL;
	}

	int r = 0;
	int fd = open(nvram_interface_path(priv, section), O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	if (fd < 0) {
		return -errno;
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

exit:
	close(fd);
	return r;
}

const char* nvram_interface_path(const struct nvram_interface_priv* priv, enum nvram_section section)
{
	switch (section) {
	case NVRAM_SECTION_A:
		return priv->a_path;
	case NVRAM_SECTION_B:
		return priv->b_path;
	default:
		return "UNDEFINED_PATH";
	}
}
