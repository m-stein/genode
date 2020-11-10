LIBS    += cbe_common spark aes_cbc_4k external

INC_DIR += $(REP_DIR)/src/lib/external_crypto

SRC_ADB += external-crypto.adb

vpath % $(REP_DIR)/src/lib/external_crypto
