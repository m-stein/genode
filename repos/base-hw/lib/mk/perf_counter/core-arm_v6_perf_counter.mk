#
# \brief  Core build-config that depends on performance-counter status
# \author Josef Soentgen
# \date   2013-09-26
#

# declare source paths
vpath % $(REP_DIR)/src/core/spec/arm_v6

# include less specific configuration
include $(REP_DIR)/lib/mk/core-perf_counter.inc
