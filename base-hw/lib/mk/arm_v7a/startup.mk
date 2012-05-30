#
# \brief  Essential platform specific sources and startup code for common programs
# \author Martin Stein
# \date   2012-04-16
#

# Add libraries
LIBS += cxx lock

# Add C++ sources
SRC_CC += _main.cc

# Add assembly sources
SRC_S += crt0.s syscall.cc

# Add include paths
INC_DIR += $(REP_DIR)/src/platform $(BASE_DIR)/src/platform

# Declare source paths
vpath crt0.s $(REP_DIR)/src/platform
vpath _main.cc $(BASE_DIR)/src/platform
vpath syscall.cc $(REP_DIR)/src/base/arm_v7a
