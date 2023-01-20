OPENSSL_DIR := $(call select_from_ports,openssl)

SRC_CC += vfs.cc
SRC_CC += aes_256.cc
SRC_CC += integer.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/cbe_nta
INC_DIR += $(OPENSSL_DIR)/include

LIBS += libcrypto

vpath % $(REP_DIR)/src/lib/vfs/cbe_nta

SHARED_LIB := yes
