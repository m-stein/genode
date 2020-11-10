LIBS += spark sha256_4k

INC_DIR += $(REP_DIR)/src/lib/cbe_common

SRC_ADB += cbe.adb
SRC_ADB += cbe-debug.adb
SRC_ADB += cbe-request.adb
SRC_ADB += cbe-primitive.adb
SRC_ADB += cbe-block_io.adb
SRC_ADB += cbe-ta_request.adb
SRC_ADB += cbe-trust_anchor.adb

vpath % $(REP_DIR)/src/lib/cbe_common

CC_ADA_OPT += -gnatec=$(REP_DIR)/src/lib/cbe_common/pragmas.adc
