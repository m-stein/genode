#
# \brief   Included by the board-specific core-support lib
# \author  Martin Stein
# \date    2012-04-27
#

# Add include paths
INC_DIR += $(BOARD_DIR) $(REP_DIR)/src/core/include $(REP_DIR)/include \
           $(BASE_DIR)/src/core/include $(BASE_DIR)/include

# Set entry point of core's first thread
CC_OPT  += -DCORE_MAIN=_main

# Add C++ sources
SRC_CC += platform_support.cc kernel.cc rm_session_support.cc

# Declare source paths
vpath platform_support.cc $(BOARD_DIR)
vpath %.cc $(REP_DIR)/src/core

