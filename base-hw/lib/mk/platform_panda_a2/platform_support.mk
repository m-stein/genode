#
# \brief   Platform implementations specific for base-hw and Panda A2
# \author  Martin Stein
# \date    2012-05-10
#

# Add include paths
INC_DIR += $(REP_DIR)/src/core/include \
           $(BASE_DIR)/src/core/include

# Add C++ sources
SRC_CC += platform_support.cc

# Declare source paths
vpath platform_support.cc $(REP_DIR)/src/core/panda_a2

