#
# \brief  Submit and receive asynchronous events
# \author Martin Stein
# \date   2012-05-07
#

# Add C++ sources
SRC_CC += signal.cc

# Add library dependencies
LIBS += slab

# Declare source paths
vpath signal.cc $(REP_DIR)/src/base/signal

