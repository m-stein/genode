#
# \brief   Parts of 'vinit' that are specific for ARM V7A
# \author  Martin Stein
# \date    2012-04-27
#

# skip build if required specs not fullfilled
REQUIRES += arm_v7a

# add include paths
INC_DIR += $(PRG_DIR)

# declare path of generic sources
GENERIC_PRG_DIR = $(PRG_DIR)/..

# include generic part of 'vinit'
include $(GENERIC_PRG_DIR)/target.inc

