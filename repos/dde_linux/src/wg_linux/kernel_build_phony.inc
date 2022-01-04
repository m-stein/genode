#
# Building the Linux kernel has its own Makefile as it is used by the
# 'wireguard_x86_64_linux' library as well.
#
# Expected variables: LX_SRC_DIR, LX_OUT_DIR
# Provided targets:   $(LX_OUT_DIR)/kernel_build.phony
#

LX_MK_ARGS = ARCH=x86_64 CROSS_COMPILE=$(CROSS_DEV_PREFIX)

#
# Linux kernel configuration
#
# Start with 'make tinyconfig', enable/disable options via 'scripts/config',
# and resolve config dependencies via 'make olddefconfig'.
#

# define 'LX_ENABLE' and 'LX_DISABLE'
include $(REP_DIR)/src/wg_linux/lx_enable_disable.inc

# filter for make output of kernel build system
BUILD_OUTPUT_FILTER = 2>&1 | sed "s/^/      [Linux]  /"

$(LX_OUT_DIR)/kernel_config.tag: $(LX_OUT_DIR)
	$(MSG_CONFIG)Linux
	$(VERBOSE)$(MAKE) -C $(LX_SRC_DIR) O=$(LX_OUT_DIR) $(LX_MK_ARGS) tinyconfig $(BUILD_OUTPUT_FILTER)
	$(VERBOSE)$(LX_SRC_DIR)/scripts/config --file $(LX_OUT_DIR)/.config $(addprefix --enable ,$(LX_ENABLE))
	$(VERBOSE)$(LX_SRC_DIR)/scripts/config --file $(LX_OUT_DIR)/.config $(addprefix --disable ,$(LX_DISABLE))
	$(VERBOSE)$(MAKE) -C $(LX_OUT_DIR) $(LX_MK_ARGS) olddefconfig $(BUILD_OUTPUT_FILTER)
	$(VERBOSE)touch $@

# update Linux kernel config on makefile changes
$(LX_OUT_DIR)/kernel_config.tag: $(MAKEFILE_LIST)

$(LX_OUT_DIR)/kernel_build.phony: $(LX_OUT_DIR)/kernel_config.tag
	$(MSG_BUILD)Linux
	$(VERBOSE)$(MAKE) -C $(LX_OUT_DIR) $(LX_MK_ARGS) $(BUILD_OUTPUT_FILTER)
