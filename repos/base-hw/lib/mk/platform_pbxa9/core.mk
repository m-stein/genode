#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/pbxa9

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += platform_support.cc

# declare source paths
vpath platform_services.cc $(BASE_DIR)/src/core
vpath platform_support.cc   $(REP_DIR)/src/core/spec/pbxa9

# include less specific library parts
include $(REP_DIR)/lib/mk/cortex_a9/core.inc
include $(REP_DIR)/lib/mk/pl011/core.inc
