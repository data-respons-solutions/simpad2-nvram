#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "libnvram.h"
#include "test-common.h"

// validate entry in list at pos index
// return 0 for equal
static int check_nvram_list_entry(const struct nvram_list* list, int index, const struct nvram_entry* entry)
{
	int cur_index = 0;
	for (struct nvram_list* cur = (struct nvram_list*) list; cur != NULL; cur = cur->next) {
		if (cur->entry) {
			if (!entrycmp(cur->entry, entry)) {
				if (index != cur_index) {
					printf("%s: found at wrong index %d != %d\n", __func__, index, cur_index);
					return 1;
				}
				return 0;
			}
		}
		cur_index++;
	}

	printf("%s: not found in list\n", __func__);
	return 1;
}

static int test_nvram_list_set()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct nvram_entry entry3;
	fill_entry(&entry3, "TEST3", "ghi");
	struct nvram_list *list = NULL;

	int r = 1;

	nvram_list_set(&list, &entry1);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &entry2);
	if (check_nvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	nvram_list_set(&list, &entry3);
	if (check_nvram_list_entry(list, 2, &entry3)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_overwrite_first()
{
	struct nvram_entry value1;
	fill_entry(&value1, "TEST1", "abc");
	struct nvram_entry value2;
	fill_entry(&value2, "TEST1", "def");

	struct nvram_list *list = NULL;
	int r = 1;

	nvram_list_set(&list, &value1);
	if (check_nvram_list_entry(list, 0, &value1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &value2);
	if (check_nvram_list_entry(list, 0, &value2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_overwrite_second()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct nvram_entry value1;
	fill_entry(&value1, "TEST2", "def");
	struct nvram_entry value2;
	fill_entry(&value2, "TEST2", "ghi");

	struct nvram_list *list = NULL;
	int r = 1;

	nvram_list_set(&list, &entry1);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &value1);
	if (check_nvram_list_entry(list, 1, &value1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &value2);
	if (check_nvram_list_entry(list, 1, &value2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");

	struct nvram_list *list = NULL;
	int r = 1;

	nvram_list_set(&list, &entry1);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	nvram_list_remove(&list, entry1.key, entry1.key_len);
	if (!check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove_first()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct nvram_list *list = NULL;
	int r = 1;

	nvram_list_set(&list, &entry1);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &entry2);
	if (check_nvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	nvram_list_remove(&list, entry1.key, entry1.key_len);
	if (check_nvram_list_entry(list, 0, &entry2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove_second()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct nvram_list *list = NULL;
	int r = 1;

	nvram_list_set(&list, &entry1);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &entry2);
	if (check_nvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	nvram_list_remove(&list, entry2.key, entry2.key_len);
	if (!check_nvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove_middle()
{
	struct nvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct nvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct nvram_entry entry3;
	fill_entry(&entry3, "TEST3", "ghi");

	struct nvram_list *list = NULL;
	int r = 1;

	nvram_list_set(&list, &entry1);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	nvram_list_set(&list, &entry2);
	if (check_nvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	nvram_list_set(&list, &entry3);
	if (check_nvram_list_entry(list, 2, &entry3)) {
		goto error_exit;
	}

	nvram_list_remove(&list, entry2.key, entry2.key_len);
	if (check_nvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	if (check_nvram_list_entry(list, 1, &entry3)) {
		goto error_exit;
	}
	if (!check_nvram_list_entry(list, 2, &entry2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

struct test test_array[] = {
		ADD_TEST(test_nvram_list_set),
		ADD_TEST(test_nvram_list_overwrite_first),
		ADD_TEST(test_nvram_list_overwrite_second),
		ADD_TEST(test_nvram_list_remove),
		ADD_TEST(test_nvram_list_remove_first),
		ADD_TEST(test_nvram_list_remove_second),
		ADD_TEST(test_nvram_list_remove_middle),
		{NULL, NULL},
};
