#
# \brief  Build configuration for specifier hw_panda
# \author Martin Stein
# \date   2011-12-20
#

# denote wich specs are also fullfilled by this spec
SPECS += hw

# configure multiprocessor mode
PROCESSORS = 1

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x81000000

# include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
