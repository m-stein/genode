LIBS += spark cbe_common cbe_cxx_common external external_trust_anchor

INC_DIR += $(REP_DIR)/src/lib/cbe
INC_DIR += $(REP_DIR)/src/lib/cbe_common
INC_DIR += $(REP_DIR)/src/lib/cbe_cxx
INC_DIR += $(REP_DIR)/src/lib/cbe_cxx_common

INC_DIR += $(REP_DIR)/src/lib/external_trust_anchor
INC_DIR += $(REP_DIR)/src/lib/external_trust_anchor_cxx

SRC_ADB += external-trust_anchor-cxx.adb

vpath % $(REP_DIR)/src/lib/external_trust_anchor_cxx

SHARED_LIB := yes

include $(REP_DIR)/lib/mk/generate_ada_main_pkg.inc
