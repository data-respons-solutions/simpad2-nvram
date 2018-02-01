
CXX ?= g++

INSTALL_PATH ?= /usr/sbin


TARGET_TYPE ?= TARGET_CARGOTEC
VPD_EEPROM_PATH = /dev/mtd1

CXXFLAGS += -D$(TARGET_TYPE) -std=c++11 -DVPD_EEPROM_PATH=$(VPD_EEPROM_PATH)

COMMON_OBJS := crc32.o filevpd.o vpd.o eeprom_vpd.o eeprom_vpd_nofs.o nvram.o

$(COMMON_OBJS): vpd.h filevpd.h eeprom_vpd.h eeprom_vpd_nofs.h Makefile

all: nvram

nvram : $(COMMON_OBJS) Makefile
	$(CXX) -o nvram  $(COMMON_OBJS) $(LDFLAGS)

install:
	install -m 0755 -D nvram $(INSTALL_PATH)/nvram

clean:
	rm -f *.o nvram
