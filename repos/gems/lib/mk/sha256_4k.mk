SRC_ADB := sha256_4k.adb
LIBS    += spark libsparkcrypto

CC_ADA_OPT += -gnatec=$(REP_DIR)/src/lib/sha256_4k/spark.adc

INC_DIR += $(REP_DIR)/src/lib/sha256_4k

sha256_4k.o : sha256_4k.ads

vpath % $(REP_DIR)/src/lib/sha256_4k
