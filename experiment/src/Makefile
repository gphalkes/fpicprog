DEBUG:=1

SOURCES.fpicprog = main.cc controller.cc driver.cc sequence_generator.cc util.cc status.cc \
	strings.cc device_db.cc program.cc high_level_controller.cc ftdi_sb.cc
LDLIBS.fpicprog := -lftdi1 -lgflags -lshlwapi

CXXTARGETS := fpicprog

COMPILER := external
CXX := x86_64-w64-mingw32-g++-posix
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../../makesys/rules.mk
#================================================#

CXXFLAGS += -std=c++14
CXXFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
CXXFLAGS += -DDEVICE_DB_PATH=\"$(CURDIR)/../device_db\"
LDFLAGS += -std=c++14

CXXFLAGS += -I../libusb/include/libusb-1.0
CXXFLAGS += -I..//install/usr/local/include/libftdi1
CXXFLAGS += -I..//install/usr/local/Include
LDFLAGS += -L../libusb/MinGW64/dll
LDFLAGS += -L../install/usr/local/lib
LDFLAGS += -L../install/usr/local/Lib
