
CXX ?= g++

PREFIX ?= /usr


TARGET_TYPE ?= TARGET_LMPLUS

CXXFLAGS += -D$(TARGET_TYPE) -std=c++11

COMMON_OBJS := crc32.o common.o filevpd.o vpd.o eeprom_vpd.o nvram.o

$(COMMON_OBJS): vpd.h filevpd.h eeprom_vpd.h Makefile

all: nvram

nvram : $(COMMON_OBJS) Makefile
	$(CXX) -o nvram  $(COMMON_OBJS) $(LDFLAGS)

install:
	install -m 0755 -D nvram $(PREFIX)/bin/nvram

clean:
	rm -f *.o nvram

