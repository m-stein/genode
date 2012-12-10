#
# \brief  Makefile for core
# \author Stefan Kalkowski
# \date   2012-10-24
#

# declare wich specs must be given to build this target
REQUIRES = platform_imx53

INC_DIR += $(REP_DIR)/src/core/include/imx53

# include less specific target parts
include $(REP_DIR)/src/core/target.inc
