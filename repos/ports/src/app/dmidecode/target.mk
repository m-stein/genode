TARGET = dmidecode

FILTER_OUT = vpddecode.c biosdecode.c ownership.c

DMIDECODE_DIR := $(call select_from_ports,dmidecode)/src/app/dmidecode

SRC_C  += dmidecode.c dmiopt.c util.c dmioem.c
SRC_CC += posix.cc

vpath %.c  $(DMIDECODE_DIR)
vpath %.cc $(PRG_DIR)

INC_DIR += $(PRG_DIR)
INC_DIR += $(DMIDECODE_DIR)

LIBS += libc

CC_CXX_WARN_STRICT =
