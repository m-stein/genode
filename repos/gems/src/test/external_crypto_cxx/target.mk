REQUIRES := x86_64

TARGET  := test-external_crypto_cxx
SRC_CC  := main.cc

# force binder for adainit
SRC_ADS := dummy.ads
LIBS    += spark

INC_DIR := $(PRG_DIR)
LIBS    += base cbe cbe_cxx sha256_4k external_crypto external_crypto_cxx
