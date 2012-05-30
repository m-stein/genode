#
# \brief  Offer configurations that are specific to base-hw and Pandaboard A2
# \author Martin Stein
# \date   2011-12-20
#

# Denote wich specs are also fullfilled by this spec
SPECS += hw platform_panda_a2

# Set address where to link text segment at
LD_TEXT_ADDR ?= 0x80000000

# Include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_panda_a2.mk)
