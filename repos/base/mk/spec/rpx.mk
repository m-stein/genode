#
# \brief  Build-system configurations for Raspberry Pi
# \author Norman Feske
# \date   2013-04-05
#

# denote wich specs are also fullfilled by this spec
SPECS += cortex_a7 usb framebuffer gpio

# add repository relative include paths
REP_INC_DIR += include/spec/rpx

# include implied specs
include $(call select_from_repositories,mk/spec/cortex_a7.mk)
include $(call select_from_repositories,mk/spec/pl011.mk)
