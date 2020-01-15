
CC ?= gcc
INSTALL_PATH ?= /usr/sbin

NVRAM_INTERFACE_TYPE ?= file
OBJS = log.o nvram.o nvram_interface.o main.o libnvram/libnvram.a

ifeq ($(NVRAM_INTERFACE_TYPE), file)
OBJS += nvram_interface_file.o
NVRAM_FACTORY_A ?= /srv/nvram/factory_a
NVRAM_FACTORY_B ?= /srv/nvram/factory_b
NVRAM_USER_A ?= /srv/nvram/user_a
NVRAM_USER_B ?= /srv/nvram/user_b
endif

ifeq ($(NVRAM_INTERFACE_TYPE), mtd)
OBJS += nvram_interface_mtd.o
LDFLAGS += -lmtd
NVRAM_FACTORY_A ?= factory_a
NVRAM_FACTORY_B ?= factory_b
NVRAM_USER_A ?= user_a
NVRAM_USER_B ?= user_b
endif

ifeq ($(NVRAM_INTERFACE_TYPE), efi)
OBJS += nvram_interface_efi.o
LDFLAGS += -le2p
NVRAM_FACTORY_A ?= a
NVRAM_FACTORY_B ?= b
NVRAM_USER_A ?= c
NVRAM_USER_B ?= d
endif

CFLAGS += -std=gnu11 -Wall -Wextra -Werror -pedantic
CFLAGS += -DNVRAM_FACTORY_A=$(NVRAM_FACTORY_A)
CFLAGS += -DNVRAM_FACTORY_B=$(NVRAM_FACTORY_B)
CFLAGS += -DNVRAM_USER_A=$(NVRAM_USER_A)
CFLAGS += -DNVRAM_USER_B=$(NVRAM_USER_B)

all: nvram
.PHONY : all

nvram : $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: libnvram/libnvram.a
libnvram/libnvram.a:
	make -C libnvram

install:
	install -m 0755 -D nvram $(INSTALL_PATH)/

clean:
	rm -f $(OBJS)
	rm -f nvram
	make -C libnvram clean
