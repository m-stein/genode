#
# \brief   Timer session server
# \author  Stefan Kalkowski
# \date    2012-10-25
#

# Set program name
TARGET = timer

# Add C++ sources
SRC_CC += main.cc

# Skip build if required specs not fullfilled
REQUIRES += hw epit

# Add libraries
LIBS += cxx server env alarm

# Add include paths
INC_DIR += $(PRG_DIR) $(PRG_DIR)/../ $(PRG_DIR)/../../nova/

# Declare source paths
vpath main.cc $(PRG_DIR)/../..
