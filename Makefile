DEBUG:=1

SOURCES.programmer = main.cc controller.cc driver.cc sequence_generator.cc util.cc status.cc \
	strings.cc device_db.cc program.cc high_level_controller.cc ftdi_sb.cc
LDLIBS.programmer := -lftdi -lgflags

CXXTARGETS := programmer
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../makesys/rules.mk
#================================================#

CXXFLAGS += -std=c++14
CXXFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
CXXFLAGS += -DDEVICE_DB_PATH=\"$(CURDIR)/device_db\"