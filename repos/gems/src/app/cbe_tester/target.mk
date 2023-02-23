REQUIRES := x86_64

TARGET  := cbe_tester
SRC_CC  += main.cc
SRC_CC  += crypto.cc
SRC_CC  += trust_anchor.cc
SRC_CC  += vfs_utilities.cc
SRC_CC  += module.cc
SRC_CC  += cbe_librara.cc
SRC_CC  += cbe_init_librara.cc

INC_DIR := $(PRG_DIR)

LIBS += base
LIBS += vfs
LIBS += cbe
LIBS += cbe_common
LIBS += cbe_cxx
LIBS += cbe_cxx_common
LIBS += cbe_init
LIBS += cbe_init_cxx
LIBS += cbe_check
LIBS += cbe_check_cxx
LIBS += cbe_dump
LIBS += cbe_dump_cxx
LIBS += libsparkcrypto
LIBS += spark

CC_CXX_WARN_STRICT_CONVERSION =
