#
# \brief  Build config for Genodes core process
# \author Norman Feske
# \date   2013-04-05
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/rpi

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += platform_support.cc

# declare source paths
vpath platform_services.cc $(BASE_DIR)/src/core
vpath platform_support.cc   $(REP_DIR)/src/core/spec/rpi

# include less specific library parts
include $(REP_DIR)/lib/mk/arm_v6/core.inc
include $(REP_DIR)/lib/mk/pl011/core.inc
