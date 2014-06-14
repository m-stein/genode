#
# \brief  Build configuration for specifier hw_panda_b
# \author Martin Stein
# \date   2014-06-14
#

# denote wich specs are also fullfilled by this spec
SPECS += hw_panda platform_panda_b

# include implied specs
include $(call select_from_repositories,mk/spec-hw_panda.mk)
include $(call select_from_repositories,mk/spec-platform_panda_b.mk)
