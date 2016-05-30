#
# \brief  Build config for Genodes core process
# \author Norman Feske
# \date   2013-04-05
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/rpx
INC_DIR += $(REP_DIR)/src/core/include/spec/pl011

BASE_HW_DIR := $(REP_DIR)

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/rpx/platform_support.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a7/core.inc
