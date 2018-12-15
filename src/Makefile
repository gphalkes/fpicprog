DEBUG:=1

SOURCES.fpicprog = fpicprog.cc pic16controller.cc picnew8bitcontroller.cc pic18controller.cc \
	pic24controller.cc driver.cc sequence_generator.cc util.cc status.cc strings.cc device_db.cc \
	program.cc high_level_controller.cc ftdi_sb.cc
LDLIBS.fpicprog := -lftdi1 -lgflags

SOURCES.testgen = testgen.cc program.cc device_db.cc strings.cc util.cc status.cc
LDLIBS.testgen := -lgflags

SOURCES.formathex = formathex.cc program.cc device_db.cc strings.cc util.cc status.cc
LDLIBS.formathex = -lgflags

CXXTARGETS := fpicprog testgen formathex
#================================================#
# NO RULES SHOULD BE DEFINED BEFORE THIS INCLUDE #
#================================================#
include ../../makesys/rules.mk
#================================================#

CXXFLAGS += -std=c++14
CXXFLAGS += -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
CXXFLAGS += -DDEVICE_DB_PATH=\"$(CURDIR)/../device_db\"
LDFLAGS += -std=c++14
