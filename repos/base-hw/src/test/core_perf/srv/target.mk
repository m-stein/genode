#
# \brief   Dummy server for measuring communication performance
# \author  Martin Stein
# \date    2016-03-23
#

# Set program name
TARGET = test-core_perf-srv

# Add C++ sources
SRC_CC = main.cc

# Add include paths
INC_DIR += $(PRG_DIR)/../include

# Add libraries
LIBS = base server
