#
# \brief   Genode thread API implementation
# \author  Martin Stein
# \date    2012-04-16
#

# Add C++ sources
SRC_CC += thread.cc thread_bootstrap.cc thread_support.cc

# Declare source paths
vpath thread_support.cc $(REP_DIR)/src/base/
vpath %.cc $(BASE_DIR)/src/base/thread/
