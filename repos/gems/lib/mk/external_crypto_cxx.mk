LIBS += spark libsparkcrypto aes_cbc_4k cbe_common cbe_cxx_common external external_crypto

INC_DIR += $(REP_DIR)/src/lib/cbe
INC_DIR += $(REP_DIR)/src/lib/cbe_common
INC_DIR += $(REP_DIR)/src/lib/cbe_cxx
INC_DIR += $(REP_DIR)/src/lib/cbe_cxx_common

INC_DIR += $(REP_DIR)/src/lib/external_crypto
INC_DIR += $(REP_DIR)/src/lib/external_crypto_cxx

SRC_ADB += external-crypto-cxx.adb

vpath % $(REP_DIR)/src/lib/external_crypto_cxx

SHARED_LIB := yes

include $(REP_DIR)/lib/mk/generate_ada_main_pkg.inc
