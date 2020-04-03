#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#include "log.h"
#include "nvram.h"
#include "libnvram/libnvram.h"

#define xstr(a) str(a)
#define str(a) #a

#define NVRAM_LOCKFILE "/run/lock/nvram.lock"
#define NVRAM_ENV_DEBUG "NVRAM_DEBUG"
#define NVRAM_ENV_FACTORY_MODE "NVRAM_FACTORY_MODE"
#define NVRAM_ENV_USER_A "NVRAM_USER_A"
#define NVRAM_ENV_USER_B "NVRAM_USER_B"
#define NVRAM_ENV_FACTORY_A "NVRAM_FACTORY_A"
#define NVRAM_ENV_FACTORY_B "NVRAM_FACTORY_B"

static int FDLOCK = 0;

static const char* get_env_str(const char* env, const char* def)
{
	const char *str = getenv(env);
	if (str) {
		return str;
	}

	return def;
}

static long get_env_long(const char* env)
{
	const char *val = getenv(env);
	if (val) {
		char *endptr = NULL;
		return strtol(val, &endptr, 10);
	}
	return 0;
}

static int acquire_lockfile(const char *path, int* fdlock)
{
	int r = 0;

	int fd = open(path, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR);
	if (fd < 0) {
		r = errno;
		pr_err("failed opening lockfile: %s [%d]: %s\n", path, r, strerror(r));
		return -r;
	}

	if (flock(fd, LOCK_EX | LOCK_NB)) {
		r = errno;
		pr_err("failed locking lockfile: %s [%d]: %s\n", path, r, strerror(r));
		close(fd);
		return -r;
	}

	*fdlock = fd;
	pr_dbg("%s: locked\n", path);

	return 0;
}

static int release_lockfile(const char* path, int fdlock)
{
	int r = 0;

	if (fdlock) {
		if(close(fdlock)) {
			r = errno;
			pr_err("failed closing lockfile: %s [%d]: %s", path, r, strerror(r));
			return -r;
		}
		if (remove(path)) {
			r = errno;
			pr_err("failed removing lockfile: %s [%d]: %s", path, r, strerror(r));
			return -r;
		}
	}

	pr_dbg("%s: unlocked\n", path);

	return 0;
}

enum op {
	OP_LIST = 0,
	OP_SET,
	OP_GET,
	OP_DEL,
};

struct opts {
	int factory_mode;
	enum op op;
	char* key;
	char* val;
};

static void print_usage(void)
{
	printf("RTFM\n");
}

int main(int argc, char** argv)
{

	struct opts opts = {0,OP_LIST,NULL,NULL};
	int r = 0;

	if (get_env_long(NVRAM_ENV_DEBUG)) {
		enable_debug();
	}
	if (get_env_long(NVRAM_ENV_FACTORY_MODE)) {
		opts.factory_mode=1;
	}

	for (int i = 0; i < argc; i++) {
		if (!strcmp("set", argv[i])) {
			if (i + 2 >= argc) {
				print_usage();
				return EINVAL;
			}
			opts.op = OP_SET;
			opts.key = argv[i + 1];
			opts.val = argv[i + 2];
			i += 2;
			break;
		}
		else
		if (!strcmp("get", argv[i])) {
			if (++i >= argc) {
				print_usage();
				return EINVAL;
			}
			opts.op = OP_GET;
			opts.key = argv[i];
			break;
		}
		else
		if (!strcmp("list", argv[i])) {
			opts.op = OP_LIST;
		}
		else
		if(!strcmp("delete", argv[i])) {
			if (++i >= argc) {
				print_usage();
				return EINVAL;
			}
			opts.op = OP_DEL;
			opts.key = argv[i];
			break;
		}
	}

	struct nvram *nvram_factory = NULL;
	struct nvram_list list_factory = NVRAM_LIST_INITIALIZER;
	struct nvram *nvram_user = NULL;
	struct nvram_list list_user = NVRAM_LIST_INITIALIZER;
	const char *nvram_factory_a = get_env_str(NVRAM_ENV_FACTORY_A, xstr(NVRAM_FACTORY_A));
	const char *nvram_factory_b = get_env_str(NVRAM_ENV_FACTORY_B, xstr(NVRAM_FACTORY_B));
	const char *nvram_user_a = get_env_str(NVRAM_ENV_USER_A, xstr(NVRAM_USER_A));
	const char *nvram_user_b = get_env_str(NVRAM_ENV_USER_B, xstr(NVRAM_USER_B));

	r = acquire_lockfile(NVRAM_LOCKFILE, &FDLOCK);
	if (r) {
		goto exit;
	}

	r = nvram_init(&nvram_factory, &list_factory, nvram_factory_a, nvram_factory_b);
	if (r) {
		goto exit;
	}
	if (!opts.factory_mode) {
		r = nvram_init(&nvram_user, &list_user, nvram_user_a, nvram_user_b);
		if (r) {
			goto exit;
		}
	}

	switch (opts.op) {
	case OP_LIST:
		pr_dbg("listing all\n");
		struct nvram_node* cur = list_factory.entry;
		while(cur) {
			printf("%s=%s\n", cur->key, cur->value);
			cur = cur->next;
		}
		if(!opts.factory_mode) {
			cur = list_user.entry;
			while(cur) {
				printf("%s=%s\n", cur->key, cur->value);
				cur = cur->next;
			}
		}
		break;
	case OP_GET:
		pr_dbg("getting: %s\n", opts.key);
		opts.val = nvram_list_get(&list_factory, opts.key);
		if (!opts.val) {
			opts.val = nvram_list_get(&list_user, opts.key);
			if (!opts.val) {
				r = -ENOENT;
				goto exit;
			}
		}
		printf("%s\n", opts.val);
		break;
	case OP_SET:
		pr_dbg("setting: %s=%s\n", opts.key, opts.val);
		if (!opts.factory_mode && nvram_list_get(&list_factory, opts.key)) {
			printf("key: %s: already in immutable factory nvram -- abort\n", opts.key);
			r = -EACCES;
			goto exit;
		}
		else {
			const char* userval = nvram_list_get(opts.factory_mode ? &list_factory : &list_user, opts.key);
			if (!userval || strcmp(userval, opts.val)) {
				r = nvram_list_set(opts.factory_mode ? &list_factory : &list_user, opts.key, opts.val);
				if (r) {
					pr_err("failed setting to list [%d]: %s\n", -r, strerror(-r));
					goto exit;
				}

				r = nvram_commit(opts.factory_mode ? nvram_factory : nvram_user, opts.factory_mode ? &list_factory : &list_user);
				if (r) {
					goto exit;
				}
			}
		}
		break;
	case OP_DEL:
		pr_dbg("deleting: %s\n", opts.key);
		if(nvram_list_remove(opts.factory_mode ? &list_factory : &list_user, opts.key)) {
			r = nvram_commit(opts.factory_mode ? nvram_factory : nvram_user, opts.factory_mode ? &list_factory : &list_user);
			if (r) {
				goto exit;
			}
		}
		break;
	}

	r = 0;

exit:
	release_lockfile(NVRAM_LOCKFILE, FDLOCK);
	destroy_nvram_list(&list_factory);
	destroy_nvram_list(&list_user);
	nvram_close(&nvram_factory);
	nvram_close(&nvram_user);
	return -r;
}
