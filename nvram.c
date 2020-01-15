#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include "log.h"
#include "nvram.h"
#include "nvram_interface.h"
#include "libnvram/libnvram.h"

struct nvram {
	enum nvram_section section;
	uint32_t counter;
	struct nvram_interface_priv *priv;
};

struct nvram_section_data {
	uint8_t* buf;
	size_t buf_size;
	uint32_t counter;
	uint32_t data_len;
	int is_valid;
};

static int read_section(const struct nvram* nvram, enum nvram_section section, struct nvram_section_data* data)
{
	int r = 0;
	size_t size = 0;
	uint32_t counter = 0;
	uint32_t data_len = 0;
	int is_valid = 0;
	uint8_t *buf = NULL;

	r = nvram_interface_size(nvram->priv, section, &size);
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
		is_valid = is_valid_nvram_section(buf, size, &data_len, &counter);
	}

	pr_dbg("section %s: valid: %s: counter: %"PRIu32": data_len: %"PRIu32"\n",
			nvram_section_str(section), is_valid ? "true" : "false", counter, data_len);

	data->buf = buf;
	data->buf_size = size;
	data->counter = counter;
	data->data_len = data_len;
	data->is_valid = is_valid;

	return 0;

error_exit:
	if (buf) {
		free(buf);
	}
	return r;
}

static enum nvram_section find_active_section(const struct nvram_section_data* section_a, const struct nvram_section_data* section_b)
{
	if (section_a->is_valid && (section_a->counter > section_b->counter)) {
		return NVRAM_SECTION_A;
	}
	else
	if (section_b->is_valid) {
		return NVRAM_SECTION_B;
	}
	else {
		return NVRAM_SECTION_UNKNOWN;
	}
}

int nvram_init(struct nvram** nvram, struct nvram_list* list, const char* section_a, const char* section_b)
{
	int r = 0;
	struct nvram_section_data data_a = {NULL,0,0,0,0};
	struct nvram_section_data data_b = {NULL,0,0,0,0};
	struct nvram *pnvram = (struct nvram*) malloc(sizeof(struct nvram));
	if (!pnvram) {
		return -ENOMEM;
	}
	memset(pnvram, 0, sizeof(struct nvram));

	r = nvram_interface_init(&pnvram->priv, section_a, section_b);
	if (r) {
		goto exit;
	}

	r = read_section(pnvram, NVRAM_SECTION_A, &data_a);
	if (r) {
		goto exit;
	}

	r = read_section(pnvram, NVRAM_SECTION_B, &data_b);
	if (r) {
		goto exit;
	}

	pnvram->section = find_active_section(&data_a, &data_b);
	switch (pnvram->section) {
	case NVRAM_SECTION_A:
		pr_dbg("section %s: %s: active\n", nvram_section_str(pnvram->section), nvram_interface_path(pnvram->priv, pnvram->section));
		r = nvram_section_deserialize(list, data_a.buf, data_a.data_len);
		if (r) {
			pr_err("failed deserializing data [%d]: %s\n", -r, strerror(-r));
			goto exit;
		}
		pnvram->counter = data_a.counter;
		break;
	case NVRAM_SECTION_B:
		pr_dbg("section %s: %s: active\n", nvram_section_str(pnvram->section), nvram_interface_path(pnvram->priv, pnvram->section));
		r = nvram_section_deserialize(list, data_b.buf, data_b.data_len);
		if (r) {
			pr_err("failed deserializing data [%d]: %s\n", -r, strerror(-r));
			goto exit;
		}
		pnvram->counter = data_b.counter;
		break;
	case NVRAM_SECTION_UNKNOWN:
		pr_dbg("no active section found\n");
		break;
	}

	r = 0;

	*nvram = pnvram;

exit:
	if (r) {
		free(pnvram);
	}
	if (data_a.buf) {
		free(data_a.buf);
	}
	if (data_b.buf) {
		free(data_b.buf);
	}

	return r;
}

int nvram_commit(struct nvram* nvram, const struct nvram_list* list)
{
	uint8_t *buf = NULL;
	uint32_t size = 0;
	int r = 0;

	r = nvram_section_serialize_size(list, &size);
	if (r) {
		pr_err("failed calculating serialized size of nvram data [%d]: %s\n", -r, strerror(-r));
		goto exit;
	}

	buf = (uint8_t*) malloc(size);
	if (!buf) {
		pr_err("failed allocating %"PRIu32" write buffer\n", size);
		r = -ENOMEM;
		goto exit;
	}

	uint32_t new_counter = nvram->counter + 1;
	r = nvram_section_serialize(list, new_counter, buf, size);
	if (r) {
		pr_err("failed serializing nvram data [%d]: %s\n", -r, strerror(-r));
		goto exit;
	}

	enum nvram_section new_section = NVRAM_SECTION_UNKNOWN;
	switch (nvram->section) {
	case NVRAM_SECTION_A:
		new_section = NVRAM_SECTION_B;
		break;
	default:
		new_section = NVRAM_SECTION_A;
		break;
	}
	pr_dbg("section %s: %s: write: %"PRIu32" b\n", nvram_section_str(new_section), nvram_interface_path(nvram->priv, new_section), size);
	r = nvram_interface_write(nvram->priv, new_section, buf, size);
	if (r) {
		pr_err("section %s: %s: failed writing %"PRIu32" bytes [%d]: %s\n",
				nvram_section_str(new_section), nvram_interface_path(nvram->priv, new_section), size, -r, strerror(-r));
		goto exit;
	}

	nvram->section = new_section;
	nvram->counter = new_counter;
	pr_dbg("section %s: valid: true: counter: %"PRIu32": data_len: %"PRIu32"\n",
			nvram_section_str(nvram->section), nvram->counter, size);
	pr_dbg("section %s: %s: active\n", nvram_section_str(nvram->section), nvram_interface_path(nvram->priv, nvram->section));

	r = 0;
exit:
	if (buf) {
		free(buf);
	}
	return r;
}

void nvram_close(struct nvram** nvram)
{
	struct nvram *pnvram = *nvram;
	if (pnvram->priv) {
		free(pnvram->priv);
	}

	if (*nvram) {
		free(*nvram);
		*nvram = NULL;
	}
}
