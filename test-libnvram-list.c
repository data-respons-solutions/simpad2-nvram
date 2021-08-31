#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "libnvram.h"
#include "test-common.h"

static int test_libnvram_list_size_0()
{
	struct libnvram_list *list = NULL;

	int r = 1;

	if (libnvram_list_size(list) != 0)
		goto error_exit;

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_size_1()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_list *list = NULL;

	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (libnvram_list_size(list) != 1)
		goto error_exit;

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_size_3()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct libnvram_entry entry3;
	fill_entry(&entry3, "TEST3", "ghi");
	struct libnvram_list *list = NULL;

	int r = 1;

	libnvram_list_set(&list, &entry1);
	libnvram_list_set(&list, &entry2);
	libnvram_list_set(&list, &entry3);
	if (libnvram_list_size(list) != 3)
		goto error_exit;

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

// validate entry in list at pos index
// return 0 for equal
static int check_libnvram_list_entry(const struct libnvram_list* list, int index, const struct libnvram_entry* entry)
{
	int cur_index = 0;
	for (struct libnvram_list* cur = (struct libnvram_list*) list; cur != NULL; cur = cur->next) {
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

static int test_libnvram_list_set()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct libnvram_entry entry3;
	fill_entry(&entry3, "TEST3", "ghi");
	struct libnvram_list *list = NULL;

	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &entry2);
	if (check_libnvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &entry3);
	if (check_libnvram_list_entry(list, 2, &entry3)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_overwrite_first()
{
	struct libnvram_entry value1;
	fill_entry(&value1, "TEST1", "abc");
	struct libnvram_entry value2;
	fill_entry(&value2, "TEST1", "def");

	struct libnvram_list *list = NULL;
	int r = 1;

	libnvram_list_set(&list, &value1);
	if (check_libnvram_list_entry(list, 0, &value1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &value2);
	if (check_libnvram_list_entry(list, 0, &value2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_overwrite_second()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_entry value1;
	fill_entry(&value1, "TEST2", "def");
	struct libnvram_entry value2;
	fill_entry(&value2, "TEST2", "ghi");

	struct libnvram_list *list = NULL;
	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &value1);
	if (check_libnvram_list_entry(list, 1, &value1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &value2);
	if (check_libnvram_list_entry(list, 1, &value2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_remove()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");

	struct libnvram_list *list = NULL;
	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	libnvram_list_remove(&list, entry1.key, entry1.key_len);
	if (!check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_remove_first()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct libnvram_list *list = NULL;
	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &entry2);
	if (check_libnvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	libnvram_list_remove(&list, entry1.key, entry1.key_len);
	if (check_libnvram_list_entry(list, 0, &entry2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_remove_second()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");

	struct libnvram_list *list = NULL;
	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &entry2);
	if (check_libnvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	libnvram_list_remove(&list, entry2.key, entry2.key_len);
	if (!check_libnvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

static int test_libnvram_list_remove_middle()
{
	struct libnvram_entry entry1;
	fill_entry(&entry1, "TEST1", "abc");
	struct libnvram_entry entry2;
	fill_entry(&entry2, "TEST2", "def");
	struct libnvram_entry entry3;
	fill_entry(&entry3, "TEST3", "ghi");

	struct libnvram_list *list = NULL;
	int r = 1;

	libnvram_list_set(&list, &entry1);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &entry2);
	if (check_libnvram_list_entry(list, 1, &entry2)) {
		goto error_exit;
	}
	libnvram_list_set(&list, &entry3);
	if (check_libnvram_list_entry(list, 2, &entry3)) {
		goto error_exit;
	}

	libnvram_list_remove(&list, entry2.key, entry2.key_len);
	if (check_libnvram_list_entry(list, 0, &entry1)) {
		goto error_exit;
	}
	if (check_libnvram_list_entry(list, 1, &entry3)) {
		goto error_exit;
	}
	if (!check_libnvram_list_entry(list, 2, &entry2)) {
		goto error_exit;
	}

	r = 0;
error_exit:
	destroy_libnvram_list(&list);
	return r;
}

struct test test_array[] = {
		ADD_TEST(test_libnvram_list_size_0),
		ADD_TEST(test_libnvram_list_size_1),
		ADD_TEST(test_libnvram_list_size_3),
		ADD_TEST(test_libnvram_list_set),
		ADD_TEST(test_libnvram_list_overwrite_first),
		ADD_TEST(test_libnvram_list_overwrite_second),
		ADD_TEST(test_libnvram_list_remove),
		ADD_TEST(test_libnvram_list_remove_first),
		ADD_TEST(test_libnvram_list_remove_second),
		ADD_TEST(test_libnvram_list_remove_middle),
		{NULL, NULL},
};
