#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include "log.h"
#include "nvram.h"
#include "nvram_interface.h"
#include "libnvram/libnvram.h"

struct nvram {
	struct libnvram_transaction trans;
	struct nvram_device *dev_a;
	struct nvram_device *dev_b;
};

static const char* nvram_active_str(enum libnvram_active active)
{
	switch (active) {
	case LIBNVRAM_ACTIVE_A:
		return "A";
	case LIBNVRAM_ACTIVE_B:
		return "B";
	default:
		return "NONE";
	}
}

static int read_section(struct nvram_device* dev, uint8_t** data, size_t* len)
{
	size_t size = 0;
	uint8_t *buf = NULL;

	int r = nvram_interface_size(dev, &size);
	if (r) {
		pr_err("%s: failed checking size [%d]: %s\n", nvram_interface_section(dev), -r, strerror(-r));
		goto error_exit;
	}

	if (size > UINT32_MAX) { // libnvram limitation
		r = -EINVAL;
		pr_err("%s: size %zu larger than limit %u\n", nvram_interface_section(dev), size, UINT32_MAX);
		goto error_exit;
	}

	if (size > 0) {
		buf = (uint8_t*) malloc(size);
		if (!buf) {
			r = -ENOMEM;
			pr_err("%s: failed allocating %zu byte read buffer\n", nvram_interface_section(dev), size);
			goto error_exit;
		}

		r = nvram_interface_read(dev, buf, size);
		if (r) {
			pr_err("%s: failed reading %zu bytes [%d]: %s\n", nvram_interface_section(dev), size, -r, strerror(-r));
			goto error_exit;
		}
	}

	*data = buf;
	*len = size;

	return 0;

error_exit:
	if (buf) {
		free(buf);
	}
	return r;
}

static int init_and_read(struct nvram_device** dev, const char* section, enum libnvram_active name, uint8_t** buf, size_t* size)
{
	pr_dbg("%s: initializing: %s\n", nvram_active_str(name), section);
	int r = nvram_interface_init(dev, section);
	if (r) {
		pr_err("%s: failed init [%d]: %s\n", section, -r, strerror(-r));
		return r;
	}
	r = read_section(*dev, buf, size);
	if (r) {
		return r;
	}
	pr_dbg("%s: size: %zu b\n", nvram_active_str(name), *size);

	return 0;
}

int nvram_init(struct nvram** nvram, struct libnvram_list** list, const char* section_a, const char* section_b)
{
	uint8_t *buf_a = NULL;
	size_t size_a = 0;
	uint8_t *buf_b = NULL;
	size_t size_b = 0;
	struct nvram *pnvram = (struct nvram*) malloc(sizeof(struct nvram));
	if (!pnvram) {
		return -ENOMEM;
	}
	memset(pnvram, 0, sizeof(struct nvram));

	int r = 0;
	if (section_a && strlen(section_a) > 0) {
		r = init_and_read(&pnvram->dev_a, section_a, LIBNVRAM_ACTIVE_A, &buf_a, &size_a);
		if (r) {
			goto exit;
		}
	}
	if (section_b && strlen(section_b) > 0) {
		r = init_and_read(&pnvram->dev_b, section_b, LIBNVRAM_ACTIVE_B, &buf_b, &size_b);
		if (r) {
			goto exit;
		}
	}

	libnvram_init_transaction(&pnvram->trans, buf_a, size_a, buf_b, size_b);
	pr_dbg("%s: active\n", nvram_active_str(pnvram->trans.active));
	r = 0;
	if ((pnvram->trans.active & LIBNVRAM_ACTIVE_A) == LIBNVRAM_ACTIVE_A) {
		r = libnvram_deserialize(list, buf_a + libnvram_header_len(), size_a - libnvram_header_len(), &pnvram->trans.section_a.hdr);
	}
	else
	if ((pnvram->trans.active & LIBNVRAM_ACTIVE_B) == LIBNVRAM_ACTIVE_B) {
		r = libnvram_deserialize(list, buf_b + libnvram_header_len(), size_b - libnvram_header_len(), &pnvram->trans.section_b.hdr);
	}

	if (r) {
		pr_err("failed deserializing data [%d]: %s\n", -r, strerror(-r));
		goto exit;
	}

	*nvram = pnvram;

exit:
	if (r) {
		if (pnvram) {
			if (pnvram->dev_a) {
				nvram_interface_destroy(&pnvram->dev_a);
			}
			if (pnvram->dev_b) {
				nvram_interface_destroy(&pnvram->dev_b);
			}
			free(pnvram);
		}
	}
	if (buf_a) {
		free(buf_a);
	}
	if (buf_b) {
		free(buf_b);
	}

	return r;
}

static int _write(struct nvram_device* dev, const uint8_t* buf, uint32_t size)
{
	pr_dbg("%s: write: %" PRIu32 " b\n", nvram_interface_section(dev), size);
	int r = nvram_interface_write(dev, buf, size);
	if (r) {
		pr_err("%s: failed writing %" PRIu32 " b [%d]: %s\n", nvram_interface_section(dev), size, -r, strerror(-r));
	}
	return r;
}

int nvram_commit(struct nvram* nvram, const struct libnvram_list* list)
{
	uint8_t *buf = NULL;
	int r = 0;
	uint32_t size = libnvram_serialize_size(list, LIBNVRAM_TYPE_LIST);

	buf = (uint8_t*) malloc(size);
	if (!buf) {
		pr_err("failed allocating %" PRIu32 " byte write buffer\n", size);
		r = -ENOMEM;
		goto exit;
	}

	struct libnvram_header hdr;
	hdr.type = LIBNVRAM_TYPE_LIST;
	enum libnvram_operation op = libnvram_next_transaction(&nvram->trans, &hdr);
	uint32_t bytes = libnvram_serialize(list, buf, size, &hdr);
	if (!bytes) {
		pr_err("failed serializing nvram data\n");
		goto exit;
	}

	if (!nvram->dev_a || !nvram->dev_b) {
		// Transactional write disabled
		r = _write(nvram->dev_a ? nvram->dev_a : nvram->dev_b, buf, size);
	}
	else {
		const int is_write_a = (op & LIBNVRAM_OPERATION_WRITE_A) == LIBNVRAM_OPERATION_WRITE_A;
		const int is_counter_reset = (op & LIBNVRAM_OPERATION_COUNTER_RESET) == LIBNVRAM_OPERATION_COUNTER_RESET;
		// first write
		r = _write(is_write_a ? nvram->dev_a : nvram->dev_b, buf, size);
		if (!r && is_counter_reset) {
			// second write, if requested
			r = _write(is_write_a ? nvram->dev_b : nvram->dev_a, buf, size);
		}
	}
	if (r) {
		goto exit;
	}

	libnvram_update_transaction(&nvram->trans, op, &hdr);

	pr_dbg("%s: active\n", nvram_active_str(nvram->trans.active));

	r = 0;
exit:
	if (buf) {
		free(buf);
	}
	return r;
}

void nvram_close(struct nvram** nvram)
{
	if (nvram && *nvram) {
		struct nvram *pnvram = *nvram;
		if (pnvram->dev_a) {
			nvram_interface_destroy(&pnvram->dev_a);
		}
		if (pnvram->dev_b) {
			nvram_interface_destroy(&pnvram->dev_b);
		}
		free(*nvram);
		*nvram = NULL;
	}
}
