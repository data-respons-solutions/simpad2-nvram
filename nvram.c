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
	struct nvram_transaction trans;
	struct nvram_interface_priv *priv;
};

static enum nvram_section_name active_to_section(enum nvram_active active)
{
	switch (active) {
	case NVRAM_ACTIVE_A:
		return NVRAM_SECTION_A;
	case NVRAM_ACTIVE_B:
		return NVRAM_SECTION_B;
	default:
		return NVRAM_SECTION_UNKNOWN;
	}
}

static int read_section(const struct nvram* nvram, enum nvram_section_name section, uint8_t** data, size_t* len)
{
	size_t size = 0;
	uint8_t *buf = NULL;

	int r = nvram_interface_size(nvram->priv, section, &size);
	if (r) {
		pr_err("section %s: %s: failed getting size [%d]: %s\n",
				nvram_section_str(section), nvram_interface_path(nvram->priv, section), -r, strerror(-r));
		goto error_exit;
	}
	pr_dbg("section %s: %s: size: %zu b\n", nvram_section_str(section), nvram_interface_path(nvram->priv, section), size);

	if (size > 0) {
		buf = (uint8_t*) malloc(size);
		if (!buf) {
			pr_err("failed allocating %zu byte read buffer\n", size);
			r = -ENOMEM;
			goto error_exit;
		}

		r = nvram_interface_read(nvram->priv, section, buf, size);
		if (r) {
			pr_err("section %s: %s: failed reading %zu b [%d]: %s\n",
					nvram_section_str(section), nvram_interface_path(nvram->priv, section), size, -r, strerror(-r));
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

int nvram_init(struct nvram** nvram, struct nvram_list** list, const char* section_a, const char* section_b)
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

	int r = nvram_interface_init(&pnvram->priv, section_a, section_b);
	if (r) {
		pr_err("failed initializing interface [%d]: %s\n", -r, strerror(-r));
		goto exit;
	}

	r = read_section(pnvram, NVRAM_SECTION_A, &buf_a, &size_a);
	if (r) {
		goto exit;
	}

	r = read_section(pnvram, NVRAM_SECTION_B, &buf_b, &size_b);
	if (r) {
		goto exit;
	}

	if (size_a > UINT32_MAX || size_b > UINT32_MAX) {
		r = -EINVAL;
		pr_err("buffer size unsupported, larger than %u bytes\n", UINT32_MAX);
		goto exit;
	}

	nvram_init_transaction(&pnvram->trans, buf_a, size_a, buf_b, size_b);
	pr_dbg("section %s: %s: active\n",
			nvram_section_str(active_to_section(pnvram->trans.active)),
			nvram_interface_path(pnvram->priv, active_to_section(pnvram->trans.active)));

	switch(pnvram->trans.active) {
	case NVRAM_ACTIVE_A:
		r = nvram_deserialize(list, buf_a + nvram_header_len(), size_a - nvram_header_len(), &pnvram->trans.section_a.hdr);
		if (r) {
			pr_err("failed deserializing data [%d]: %s\n", -r, strerror(-r));
			goto exit;
		}
		break;
	case NVRAM_ACTIVE_B:
		r = nvram_deserialize(list, buf_b + nvram_header_len(), size_b - nvram_header_len(), &pnvram->trans.section_b.hdr);
		if (r) {
			pr_err("failed deserializing data [%d]: %s\n", -r, strerror(-r));
			goto exit;
		}
		break;
	case NVRAM_ACTIVE_NONE:
		break;
	}

	r = 0;

	*nvram = pnvram;

exit:
	if (r) {
		free(pnvram);
	}
	if (buf_a) {
		free(buf_a);
	}
	if (buf_b) {
		free(buf_b);
	}

	return r;
}

int nvram_commit(struct nvram* nvram, const struct nvram_list* list)
{
	uint8_t *buf = NULL;
	int r = 0;
	uint32_t size = nvram_serialize_size(list);

	buf = (uint8_t*) malloc(size);
	if (!buf) {
		pr_err("failed allocating %"PRIu32" write buffer\n", size);
		r = -ENOMEM;
		goto exit;
	}

	struct nvram_header hdr;
	enum nvram_active next = nvram_next_transaction(&nvram->trans, &hdr);
	if (hdr.counter == UINT32_MAX) {
		pr_err("counter == %u support not implemented\n", hdr.counter);
		goto exit;
	}
	uint32_t bytes = nvram_serialize(list, buf, size, &hdr);
	if (!bytes) {
		pr_err("failed serializing nvram data\n");
		goto exit;
	}

	pr_dbg("section %s: %s: write: %"PRIu32" b\n", nvram_section_str(active_to_section(next)),
			nvram_interface_path(nvram->priv, active_to_section(next)), size);
	r = nvram_interface_write(nvram->priv, active_to_section(next), buf, size);
	if (r) {
		pr_err("section %s: %s: failed writing %"PRIu32" bytes [%d]: %s\n",
				nvram_section_str(active_to_section(next)), nvram_interface_path(nvram->priv, active_to_section(next)), size, -r, strerror(-r));
		goto exit;
	}

	nvram->trans.active = next;
	memcpy(next == NVRAM_ACTIVE_A ? &nvram->trans.section_a.hdr : &nvram->trans.section_b.hdr, &hdr, sizeof(nvram->trans.section_a.hdr));

	pr_dbg("section %s: valid: true: counter: %"PRIu32": data_len: %"PRIu32"\n",
			nvram_section_str(next), hdr.counter, size);
	pr_dbg("section %s: %s: active\n", nvram_section_str(next), nvram_interface_path(nvram->priv, active_to_section(next)));

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
		if (pnvram->priv) {
			free(pnvram->priv);
		}

		if (*nvram) {
			free(*nvram);
			*nvram = NULL;
		}
	}
}
