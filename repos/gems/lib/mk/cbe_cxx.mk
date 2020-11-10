LIBS += spark libsparkcrypto sha256_4k cbe cbe_common cbe_cxx_common

INC_DIR += $(REP_DIR)/src/lib/cbe
INC_DIR += $(REP_DIR)/src/lib/cbe_common
INC_DIR += $(REP_DIR)/src/lib/cbe_cxx
INC_DIR += $(REP_DIR)/src/lib/cbe_cxx_common

SRC_ADB += cbe-cxx-cxx_library.adb

vpath % $(REP_DIR)/src/lib/cbe_cxx

SHARED_LIB := yes

include $(REP_DIR)/lib/mk/generate_ada_main_pkg.inc
