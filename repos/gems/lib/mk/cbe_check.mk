LIBS    += spark sha256_4k cbe_common

INC_DIR += $(REP_DIR)/src/lib/cbe_check

SRC_ADB += cbe-check_library.adb
SRC_ADB += cbe-superblock_check.adb
SRC_ADB += cbe-vbd_check.adb
SRC_ADB += cbe-free_tree_check.adb

vpath % $(REP_DIR)/src/lib/cbe_check

CC_ADA_OPT += -gnatec=$(REP_DIR)/src/lib/cbe_check/pragmas.adc
