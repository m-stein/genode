#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Martin Stein
# \date   2013-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/cortex_a8

# include less specific library parts
include $(REP_DIR)/lib/mk/core-trustzone.inc
