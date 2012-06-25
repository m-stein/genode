#
# \brief   Parts of core that depend on the Versatile VEA9X4
# \author  Martin Stein
# \date    2012-04-27
#

# Declare location of core files that are board specific
BOARD_DIR = $(REP_DIR)/src/core/vea9x4

SRC_S     = monitor_vector.s

# Include generic part of core support
include $(REP_DIR)/lib/mk/core_support.inc

vpath monitor_vector.s $(REP_DIR)/src/core/arm_v7a

