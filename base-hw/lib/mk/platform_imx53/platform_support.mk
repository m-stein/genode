#
# \brief   Platform implementations specific for base-hw and i.MX53
# \author  Stefan Kalkowski
# \date    2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/include \
           $(REP_DIR)/src/core/include/imx53 \
           $(BASE_DIR)/src/core/include

# add C++ sources
SRC_CC += platform_support.cc platform_services.cc

# declare source paths
vpath platform_support.cc   $(REP_DIR)/src/core/imx53
vpath platform_services.cc  $(BASE_DIR)/src/core
