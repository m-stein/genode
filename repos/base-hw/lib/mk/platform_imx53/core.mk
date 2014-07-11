#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add library dependencies
LIBS += core-imx53_trustzone

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/imx53

# include less specific library parts
include $(REP_DIR)/lib/mk/cortex_a8/core.inc
