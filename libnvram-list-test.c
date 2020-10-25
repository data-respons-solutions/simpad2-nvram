#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include "libnvram.h"

static int check_nvram_list_entry(const struct nvram_list* list, int index, const char* key, const char* value)
{
	struct nvram_node* cur = list->entry;
	for (int i = 0; cur; i++) {
		if (!strcmp(cur->key, key)) {
			if (!strcmp(cur->value, value)) {
				if (index != i) {
					printf("%s: %s: found at wrong index %d != %d\n", __func__, key, index, i);
					return 1;
				}
				return 0;
			}
			else {
				printf("%s: %s: value mismatch %s != %s\n", __func__, key, value, cur->value);
				return 1;
			}
		}
		cur = cur->next;
	}

	printf("%s: %s not found in list\n", __func__, key);
	return 1;
}

static int test_nvram_list_set()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";
	const char* test_key3 = "TEST3";
	const char* test_val3 = "ghi";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key3, test_val3);
	if (check_nvram_list_entry(&list, 2, test_key3, test_val3)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_overwrite_first()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_val2 = "def";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key1, test_val2);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val2)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_overwrite_second()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";
	const char* test_val3 = "ghi";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val3);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val3)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_remove(&list, test_key1);
	if (!check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove_first()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_remove(&list, test_key1);
	if (check_nvram_list_entry(&list, 0, test_key2, test_val2)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove_second()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";

	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_remove(&list, test_key2);
	if (!check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

static int test_nvram_list_remove_middle()
{
	const char* test_key1 = "TEST1";
	const char* test_val1 = "abc";
	const char* test_key2 = "TEST2";
	const char* test_val2 = "def";
	const char* test_key3 = "TEST3";
	const char* test_val3 = "ghi";


	struct nvram_list list = NVRAM_LIST_INITIALIZER;
	int r = 0;

	nvram_list_set(&list, test_key1, test_val1);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key2, test_val2);
	if (check_nvram_list_entry(&list, 1, test_key2, test_val2)) {
		goto error_exit;
	}
	nvram_list_set(&list, test_key3, test_val3);
	if (check_nvram_list_entry(&list, 2, test_key3, test_val3)) {
		goto error_exit;
	}

	nvram_list_remove(&list, test_key2);
	if (check_nvram_list_entry(&list, 0, test_key1, test_val1)) {
		goto error_exit;
	}
	if (check_nvram_list_entry(&list, 1, test_key3, test_val3)) {
		goto error_exit;
	}
	if (!check_nvram_list_entry(&list, 2, test_key2, test_val2)) {
		goto error_exit;
	}

	r = 1;
error_exit:
	destroy_nvram_list(&list);
	return r;
}

struct test {
	char* name;
	int (*func)(void);
};

#define FUNC_NAME(NAME) #NAME

struct test test_array[] = {
		{FUNC_NAME(test_nvram_list_set), &test_nvram_list_set},
		{FUNC_NAME(test_nvram_list_overwrite_first), &test_nvram_list_overwrite_first},
		{FUNC_NAME(test_nvram_list_overwrite_second), &test_nvram_list_overwrite_second},
		{FUNC_NAME(test_nvram_list_remove), &test_nvram_list_remove},
		{FUNC_NAME(test_nvram_list_remove_first), &test_nvram_list_remove_first},
		{FUNC_NAME(test_nvram_list_remove_second), &test_nvram_list_remove_second},
		{FUNC_NAME(test_nvram_list_remove_middle), &test_nvram_list_remove_middle},
		{NULL, NULL},
};

int main(int argc, char** argv)
{
	(void) argc;
	(void) argv;

	int errors = 0;

	for (int i = 0; test_array[i].name; ++i) {
		int r = (*test_array[i].func)();
		if (!r) {
			errors++;
		}
		printf("%s: %s\n", test_array[i].name, r ? "PASS" : "FAIL");
	}

	if(errors) {
		return 1;
	}
	return 0;
}
