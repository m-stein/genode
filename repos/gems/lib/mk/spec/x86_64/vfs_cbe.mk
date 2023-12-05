SRC_CC += vfs.cc
SRC_CC += hash.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/cbe

LIBS += cbe_cxx libcrypto

vpath % $(REP_DIR)/src/lib/vfs/cbe

SHARED_LIB := yes

CC_CXX_WARN_STRICT :=
