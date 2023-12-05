SRC_CC += vfs.cc
SRC_CC += aes_cbc.cc

vpath % $(REP_DIR)/src/lib/vfs/tresor_crypto

INC_DIR += $(REP_DIR)/src/lib/vfs/tresor_crypto
INC_DIR += $(REP_DIR)/src/lib/tresor/include

LIBS += aes_cbc_4k

INC_DIR += $(REP_DIR)/src/lib/tresor/include
LIBS += libcrypto
SRC_CC  += hash.cc
vpath hash.cc $(REP_DIR)/src/lib/tresor

SHARED_LIB = yes
