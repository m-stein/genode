#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a8

# include less specific library parts
include $(REP_DIR)/lib/mk/arm_v7/core.inc
