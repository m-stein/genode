#
# \brief  Make configurations specific for the ARM Cortex A9
# \author Martin Stein
# \date   2011-12-20
#

# Denote wich specs are also fullfilled by this spec
SPECS += arm_v7a pl390

#
# Configure target CPU
#
CC_OPT += -mcpu=cortex-a9

# Add repository relative include paths
REP_INC_DIR += include/cortex_a9

# Include implied specs
include $(call select_from_repositories,mk/spec-arm_v7a.mk)
include $(call select_from_repositories,mk/spec-pl390.mk)
