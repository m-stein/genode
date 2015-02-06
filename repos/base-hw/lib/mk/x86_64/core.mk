#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/x86_64
INC_DIR += $(REP_DIR)/src/core/include/spec/x86

# add assembly sources
SRC_S += spec/x86_64/mode_transition.s
SRC_S += spec/x86_64/crt0.s

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/x86/platform_support.cc
SRC_CC += spec/x86_64/kernel/thread_base.cc

# use dummy boot-modules per default
BOOT_MODULES_VPATH = $(REP_DIR)/src/core/spec/x86_64

# include less specific configuration
include $(REP_DIR)/lib/mk/core.inc
