
PREFIX ?= /usr/local

CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Werror
CFLAGS += -std=gnu11
CFLAGS += -pedantic

all: libnvram.a

libnvram.a: crc32.o libnvram.o 
	$(AR) rcs $@ $^

libnvram-test: libnvram-test.o libnvram.a
	$(CC) -o $@ $^ $(LDFLAGS)
   
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test
test: libnvram-test

.PHONY: clean
clean:
	rm -f crc32.o
	rm -f libnvram.o
	rm -f libnvram.a
	rm -f libnvram-test.o
	rm -f libnvram-test
	