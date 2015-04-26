
CXX ?= g++

OBJS := vpdgenerate.o crc32.o
EX_CXXFLAGS := -std=c++11
PREFIX ?= /usr/local

LIBS := -luuid


TARGET_TYPE ?= DESKTOP
CXXFLAGS += -DTARGET_TYPE=$(TARGET_TYPE)

COMMON_OBJS := crc32.o common.o filevpd.o vpd.o

all: nvram

%.o : %.cpp Makefile
	$(CXX) -g $(CXXFLAGS) $(EX_CXXFLAGS) -c $<

	
nvram : nvram.o $(COMMON_OBJS) Makefile
	$(CXX) -g -o nvram nvram.o $(COMMON_OBJS) $(LDFLAGS) $(LIBS)

install:
	install -m 0755 -D nvram $(PREFIX)/bin/nvram

clean:
	rm -f *.o nvram

