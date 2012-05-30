
# Denote wich specs are also fullfilled by this spec
SPECS += hw platform_pbxa9

# Set address where to link text segment at
LD_TEXT_ADDR ?= 0x01000000

# Include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_pbxa9.mk)
