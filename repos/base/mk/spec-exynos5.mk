#
# \brief  Build-system configurations for Odroid XU
# \author Stefan Kalkowski
# \date   2013-11-25
#

# denote specs that are fullfilled by this spec
SPECS += cortex_a15 framebuffer usb

# add repository relative paths
REP_INC_DIR += include/exynos5

# include implied specs
include $(call select_from_repositories,mk/spec-cortex_a15.mk)
