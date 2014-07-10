#
# \brief  Build config that enables performance counting
# \author Josef Soentgen
# \date   2013-09-26
#

# add C++ sources
SRC_CC = perf_counter.cc

# declare source paths
vpath perf_counter.cc $(REP_DIR)/src/core/spec/arm_v7
