#
# A minimalistic Linux appliance tailored to creating a Wireguard tunnel
#

TARGET   := wg_linux
REQUIRES := x86_64

LX_SRC_DIR := $(call select_from_ports,linux)/src/linux
LX_OUT_DIR := $(shell pwd)

include $(REP_DIR)/src/wg_linux/kernel_build_phony.inc

CUSTOM_TARGET_DEPS += $(LX_OUT_DIR)/kernel_build.phony
