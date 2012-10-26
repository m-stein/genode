#
# \brief   Parts of core that depend on i.MX53
# \author  Stefan Kalkowski
# \date    2012-10-24
#

# declare location of core files that are board specific
BOARD_DIR = $(REP_DIR)/src/core/imx53

# add includes to search path
INC_DIR += $(REP_DIR)/src/core/include/imx53

# include less specific parts
include $(REP_DIR)/lib/mk/arm_v7/core_support.inc
include $(REP_DIR)/lib/mk/core_support.inc
