LIBS    += cbe_common spark external

INC_DIR += $(REP_DIR)/src/lib/external_trust_anchor

SRC_ADB += external-trust_anchor.adb

vpath % $(REP_DIR)/src/lib/external_trust_anchor
