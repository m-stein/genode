REQUIRES := x86_64

TARGET  := cbe_tester
SRC_CC  += main.cc
SRC_CC  += trust_anchor.cc
SRC_CC  += vfs_utilities.cc
SRC_CC  += module.cc
SRC_CC  += crypto.cc
SRC_CC  += cbe_librara.cc

INC_DIR := $(PRG_DIR)
LIBS    += base cbe_cxx cbe_init_cxx cbe_cxx_common cbe_check_cxx cbe_dump_cxx cbe vfs
LIBS    += spark libsparkcrypto

CC_CXX_WARN_STRICT_CONVERSION =
