#
# \brief  Synchronisation through locks
# \author Martin Stein
# \date   2012-04-16
#

# Add C++ sources
SRC_CC   = lock.cc

# Add include paths
INC_DIR += $(REP_DIR)/src/base/lock

# Declare source paths
vpath lock.cc $(BASE_DIR)/src/base/lock
