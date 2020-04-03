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

#define NVRAM_PROGRAM_NAME "nvram"
#define NVRAM_LOCKFILE "/run/lock/nvram.lock"
#define NVRAM_ENV_DEBUG "NVRAM_DEBUG"
#define NVRAM_ENV_USER_A "NVRAM_USER_A"
#define NVRAM_ENV_USER_B "NVRAM_USER_B"
#define NVRAM_ENV_SYSTEM_A "NVRAM_SYSTEM_A"
#define NVRAM_ENV_SYSTEM_B "NVRAM_SYSTEM_B"
#define NVRAM_ENV_SYSTEM_UNLOCK "NVRAM_SYSTEM_UNLOCK"
#define NVRAM_SYSTEM_UNLOCK_MAGIC "16440"
#define NVRAM_SYSTEM_PREFIX "SYS_"

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

static int system_unlocked(void)
{
	const char* unlock_str = getenv(NVRAM_ENV_SYSTEM_UNLOCK);
	if (unlock_str && strcmp(unlock_str, xstr(NVRAM_SYSTEM_UNLOCK_MAGIC))) {
		return 1;
	}
	return 0;
}

static int starts_with(const char* str, const char* prefix)
{
	size_t str_len = strlen(str);
	size_t prefix_len = strlen(prefix);
	if (str_len > prefix_len) {
		if(!strncmp(str, prefix, prefix_len)) {
			return 1;
		}
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
	int system_mode;
	enum op op;
	char* key;
	char* val;
};

static void print_usage(const char* progname)
{
	printf("%s, nvram interface, Data Respons Solutions AB\n", progname);
	printf("Version: %s\n", xstr(SRC_VERSION));
	printf("Usage:   %s [OPTION] [COMMAND] [KEY] [VALUE]\n", progname);
	printf("Example: %s set keyname value\n", progname);
	printf("Defaults to COMMAND list if none set\n");
	printf("\n");

	printf("Options:\n");
	printf("  --sys       ignore user section\n");
	printf("\n");

	printf("Commands:\n");
	printf("  set         Write attribute. Requires KEY And VALUE\n");
	printf("  get         Read attribute. Requires KEY\n");
	printf("  delete      Delete attributes. Requires KEY\n");
	printf("  list        Lists attributes\n");
	printf("\n");

	printf("Return values:\n");
	printf("  0 if ok\n");
	printf("  errno for error\n");
	printf("\n");
}

int main(int argc, char** argv)
{

	struct opts opts = {0,OP_LIST,NULL,NULL};
	int r = 0;

	if (get_env_long(NVRAM_ENV_DEBUG)) {
		enable_debug();
	}

	if (argc > 5) {
		fprintf(stderr, "Too many arguments\n");
		return EINVAL;
	}

	for (int i = 0; i < argc; i++) {
		if (!strcmp("set", argv[i])) {
			if (i + 2 >= argc) {
				fprintf(stderr, "Too few arguments for command set\n");
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
				fprintf(stderr, "Too few arguments for command get\n");
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
				fprintf(stderr, "Too few arguments for command delete\n");
				return EINVAL;
			}
			opts.op = OP_DEL;
			opts.key = argv[i];
			break;
		}
		else
		if (!strcmp("--sys", argv[i])) {
			opts.system_mode = 1;
		}
		else
		if (!strcmp("--help", argv[i])) {
			print_usage(NVRAM_PROGRAM_NAME);
			return 0;
		}
	}

	pr_dbg("operation: %d, key: %s, val: %s, system_mode: %d\n", opts.op, opts.key, opts.val, opts.system_mode);

	if (opts.system_mode) {
		switch (opts.op) {
		case OP_SET:
			if (!starts_with(opts.key, NVRAM_SYSTEM_PREFIX)) {
				pr_err("required prefix \"%s\" missing in system attribute\n", NVRAM_SYSTEM_PREFIX);
				return EINVAL;
			}
			//FALLTHROUGH
		case OP_DEL:
			if (!system_unlocked()) {
				pr_err("system write locked\n")
				return EACCES;
			}
			break;
		default:
			break;
		}
	}
	else {
		if (opts.op == OP_SET) {
			if (starts_with(opts.key, NVRAM_SYSTEM_PREFIX)) {
				pr_err("forbidden prefix \"%s\" in user attribute\n", NVRAM_SYSTEM_PREFIX);
				return EINVAL;
			}
		}
	}

	struct nvram *nvram_system = NULL;
	struct nvram_list list_system = NVRAM_LIST_INITIALIZER;
	struct nvram *nvram_user = NULL;
	struct nvram_list list_user = NVRAM_LIST_INITIALIZER;

	r = acquire_lockfile(NVRAM_LOCKFILE, &FDLOCK);
	if (r) {
		goto exit;
	}

	const char *nvram_system_a = get_env_str(NVRAM_ENV_SYSTEM_A, xstr(NVRAM_SYSTEM_A));
	const char *nvram_system_b = get_env_str(NVRAM_ENV_SYSTEM_B, xstr(NVRAM_SYSTEM_B));
	pr_dbg("NVRAM_SYSTEM_A: %s\n", nvram_system_a);
	pr_dbg("NVRAM_SYSTEM_B: %s\n", nvram_system_b);

	r = nvram_init(&nvram_system, &list_system, nvram_system_a, nvram_system_b);
	if (r) {
		goto exit;
	}
	if (!opts.system_mode) {
		const char *nvram_user_a = get_env_str(NVRAM_ENV_USER_A, xstr(NVRAM_USER_A));
		const char *nvram_user_b = get_env_str(NVRAM_ENV_USER_B, xstr(NVRAM_USER_B));
		pr_dbg("NVRAM_USER_A: %s\n", nvram_user_a);
		pr_dbg("NVRAM_USER_B: %s\n", nvram_user_b);
		r = nvram_init(&nvram_user, &list_user, nvram_user_a, nvram_user_b);
		if (r) {
			goto exit;
		}
	}

	switch (opts.op) {
	case OP_LIST:
		pr_dbg("listing system\n");
		struct nvram_node* cur = list_system.entry;
		while(cur) {
			printf("%s=%s\n", cur->key, cur->value);
			cur = cur->next;
		}
		if(!opts.system_mode) {
			pr_dbg("listing user\n");
			cur = list_user.entry;
			while(cur) {
				printf("%s=%s\n", cur->key, cur->value);
				cur = cur->next;
			}
		}
		break;
	case OP_GET:
		pr_dbg("getting key from system: %s\n", opts.key);
		opts.val = nvram_list_get(&list_system, opts.key);
		if (!opts.val && !opts.system_mode) {
			pr_dbg("getting key from user: %s\n", opts.key);
			opts.val = nvram_list_get(&list_user, opts.key);
		}
		if (!opts.val) {
			pr_dbg("key not found: %s\n", opts.key);
			r = -ENOENT;
			goto exit;
		}
		printf("%s\n", opts.val);
		break;
	case OP_SET:
		pr_dbg("setting %s: %s=%s\n", opts.system_mode ? "system" : "user", opts.key, opts.val);
		const char* val = nvram_list_get(opts.system_mode ? &list_system : &list_user, opts.key);
		if (!val || strcmp(val, opts.val)) {
			r = nvram_list_set(opts.system_mode ? &list_system : &list_user, opts.key, opts.val);
			if (r) {
				pr_err("failed setting to list [%d]: %s\n", -r, strerror(-r));
				goto exit;
			}

			r = nvram_commit(opts.system_mode ? nvram_system : nvram_user, opts.system_mode ? &list_system : &list_user);
			if (r) {
				goto exit;
			}
		}
		break;
	case OP_DEL:
		pr_dbg("deleting %s: %s\n", opts.system_mode ? "system" : "user", opts.key);
		if(nvram_list_remove(opts.system_mode ? &list_system : &list_user, opts.key)) {
			r = nvram_commit(opts.system_mode ? nvram_system : nvram_user, opts.system_mode ? &list_system : &list_user);
			if (r) {
				goto exit;
			}
		}
		break;
	}

	r = 0;

exit:
	release_lockfile(NVRAM_LOCKFILE, FDLOCK);
	destroy_nvram_list(&list_system);
	destroy_nvram_list(&list_user);
	nvram_close(&nvram_system);
	nvram_close(&nvram_user);
	return -r;
}
