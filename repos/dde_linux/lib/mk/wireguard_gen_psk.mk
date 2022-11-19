PORT_DIR := $(call select_from_ports,sc_hsm_embedded)/src/sc_hsm_embedded

SRC_C += generate_psk.c
SRC_C += $(shell cd $(PORT_DIR)/src; find common/*.c)

LIBS += stdcxx libc posix libssl

CC_OPT += -DPACKAGE_NAME=\"sc-hsm-embedded\"
CC_OPT += -DPACKAGE_TARNAME=\"sc-hsm-embedded\"
CC_OPT += -DPACKAGE_VERSION=\"2.11\"
CC_OPT += -DPACKAGE_STRING=\"sc-hsm-embedded\ 2.11\"
CC_OPT += -DPACKAGE_BUGREPORT=\"\"
CC_OPT += -DPACKAGE_URL=\"\"
CC_OPT += -DPACKAGE=\"sc-hsm-embedded\"
CC_OPT += -DVERSION=\"2.11\"
CC_OPT += -DSTDC_HEADERS=1
CC_OPT += -DHAVE_SYS_TYPES_H=1
CC_OPT += -DHAVE_SYS_STAT_H=1
CC_OPT += -DHAVE_STDLIB_H=1
CC_OPT += -DHAVE_STRING_H=1
CC_OPT += -DHAVE_MEMORY_H=1
CC_OPT += -DHAVE_STRINGS_H=1
CC_OPT += -DHAVE_INTTYPES_H=1
CC_OPT += -DHAVE_STDINT_H=1
CC_OPT += -DHAVE_UNISTD_H=1
CC_OPT += -DHAVE_DLFCN_H=1
CC_OPT += -DLT_OBJDIR=\".libs/\"
CC_OPT += -DENABLE_LIBCRYPTO=1
CC_OPT += -DVERSION_MAJOR=2
CC_OPT += -DVERSION_MINOR=11
CC_OPT += -MD
CC_OPT += -MP

INC_DIR += $(PORT_DIR)/src
INC_DIR += $(PRG_DIR)

vpath generate_psk.c $(REP_DIR)/src/lib/wireguard_gen_psk
vpath % $(PORT_DIR)/src

SHARED_LIB = yes
