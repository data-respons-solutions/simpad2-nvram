
CXX ?= g++

PREFIX ?= /usr/local

LIBS := -luuid


TARGET_TYPE ?= TARGET_LMPLUS

ifeq ($(TARGET_TYPE),TARGET_LMPLUS)
	CXXFLAGS += -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 --sysroot=/opt/poky/1.8/sysroots/cortexa9hf-vfp-neon-poky-linux-gnueabi
	LDFLAGS += -march=armv7-a -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9 --sysroot=/opt/poky/1.8/sysroots/cortexa9hf-vfp-neon-poky-linux-gnueabi
endif

CXXFLAGS += -D$(TARGET_TYPE)
WP_GPIO ?= -1
CXXFLAGS += -g -O0 -DWP_GPIO=$(WP_GPIO) -std=c++11

COMMON_OBJS := crc32.o common.o filevpd.o vpd.o MtdVpd.o

all: nvram

nvram : $(COMMON_OBJS) Makefile nvram.o
	$(CXX) $(CXXFLAGS) -g -o nvram nvram.o $(COMMON_OBJS) $(LDFLAGS) $(LIBS)

install:
	install -m 0755 -D nvram $(PREFIX)/bin/nvram

clean:
	rm -f *.o nvram

