#
# Helper library for server/wireguard/target.mk for building the Linux tree
# before having to determine the wireguard source files (depends on the
# output of the Linux build).
#

LX_CONTRIB_DIR := $(call select_from_ports,linux)/src/linux
LX_SRC_DIR     := $(BUILD_BASE_DIR)/server/wireguard/linux
LX_OUT_DIR     := $(BUILD_BASE_DIR)/server/wireguard/linux

$(LX_OUT_DIR):
	$(VERBOSE)rm -rf $(LX_SRC_DIR)
	$(VERBOSE)mkdir -p $(LX_SRC_DIR)
	$(VERBOSE)cp -r $(LX_CONTRIB_DIR)/* $(LX_SRC_DIR)

include $(REP_DIR)/src/wg_linux/kernel_build_phony.inc

CUSTOM_TARGET_DEPS += $(LX_OUT_DIR)/kernel_build.phony
