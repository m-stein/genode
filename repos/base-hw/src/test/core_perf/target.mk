#
# \brief   Test the performance of Core and the underlying Kernel
# \author  Martin Stein
# \date    2016-03-24
#

# Set program name
TARGET = test-core_perf

# Add C++ sources
SRC_CC += main.cc

# Add include paths
INC_DIR += $(PRG_DIR)/include

# Add libraries
LIBS += base
