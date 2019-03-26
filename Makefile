
INSTALL_PATH ?= /usr/sbin

VPD_EEPROM_PATH ?= /dev/mtd1

CXX ?= g++
CXXFLAGS += -std=c++11 
CXXFLAGS_FILE = -DTARGET_FILE
CXXFLAGS_EFI = -DTARGET_EFI
CXXFLAGS_LEGACY = -DTARGET_LEGACY -DVPD_EEPROM_PATH=$(VPD_EEPROM_PATH)
CXXFLAGS_SPERRE = -DTARGET_SPERRE -DVPD_EEPROM_PATH=$(VPD_EEPROM_PATH)

COMMON_OBJS := vpd.o
$(COMMON_OBJS): vpd.h vpdstorage.h crc32.h eeprom_vpd.h eeprom_vpd_nofs.h efivpd.h filevpd.h Makefile

all: nvram_file nvram_efi nvram_legacy nvram_sperre
.PHONY : all

nvram_file.o : nvram.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_FILE) -c -o $@ $<
	
nvram_efi.o : nvram.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_EFI) -c -o $@ $<

nvram_legacy.o : nvram.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_LEGACY) -c -o $@ $<
	
nvram_sperre.o : nvram.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_SPERRE) -c -o $@ $<

nvram_file : $(COMMON_OBJS) nvram_file.o filevpd.o
	$(CXX) -o $@ $^ $(LDFLAGS)
	
nvram_efi : $(COMMON_OBJS) nvram_efi.o efivpd.o
	$(CXX) -o $@ $^ $(LDFLAGS) -le2p
	
nvram_legacy : $(COMMON_OBJS) nvram_legacy.o eeprom_vpd.o crc32.o
	$(CXX) -o $@ $^ $(LDFLAGS)
	
nvram_sperre : $(COMMON_OBJS) nvram_sperre.o eeprom_vpd_nofs.o crc32.o
	$(CXX) -o $@ $^ $(LDFLAGS)
	
install:
	install -m 0755 -D nvram_file $(INSTALL_PATH)/nvram_file
	install -m 0755 -D nvram_efi $(INSTALL_PATH)/nvram_efi
	install -m 0755 -D nvram_legacy $(INSTALL_PATH)/nvram_legacy
	install -m 0755 -D nvram_sperre $(INSTALL_PATH)/nvram_sperre

clean:
	rm -f *.o nvram_file nvram_efi nvram_legacy nvram_sperre
