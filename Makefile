BUILD ?= build
CLANG_TIDY ?= no
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Werror
CFLAGS += -std=gnu11
CFLAGS += -pedantic

ifeq ($(abspath $(BUILD)),$(shell pwd)) 
$(error "ERROR: Build dir can't be equal to source dir")
endif

all: libnvram

.PHONY: libnvram
libnvram: $(BUILD)/libnvram.a

$(BUILD)/libnvram.a: $(addprefix $(BUILD)/, crc32.o libnvram.o)
	$(AR) rcs $@ $^

$(BUILD)/test-core: $(addprefix $(BUILD)/, test-core.o libnvram.a test-common.o)
	$(CC) -o $@ $^ $(LDFLAGS)
	
$(BUILD)/test-libnvram-list: $(addprefix $(BUILD)/, test-libnvram-list.o libnvram.a test-common.o)
	$(CC) -o $@ $^ $(LDFLAGS)
	
$(BUILD)/test-transactional: $(addprefix $(BUILD)/, test-transactional.o libnvram.a test-common.o)
	$(CC) -o $@ $^ $(LDFLAGS)
	
$(BUILD)/test-crc32: $(addprefix $(BUILD)/, test-crc32.o crc32.o test-common.o)
	$(CC) -o $@ $^ $(LDFLAGS)
   
$(BUILD)/%.o: %.c 
ifeq ($(CLANG_TIDY),yes)
	clang-tidy $< -header-filter=.* \
		-checks=-*,clang-analyzer-*,bugprone-*,cppcoreguidelines-*,portability-*,readability-* -- $<
endif
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test
test: $(addprefix $(BUILD)/, test-core test-libnvram-list test-transactional test-crc32)
	for test in $^; do \
		echo "Running: $${test}"; \
		if ! ./$${test}; then \
			exit 1; \
		fi \
	done
