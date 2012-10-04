
#
# \brief   Emulation enviroment for verilated verilog designs
# \author  Martin Stein
# \date    2012-07-15
#

# add C++ source files
SRC_CC += main.cc

# add library dependencies
LIBS += server libm stdcxx libc libc_log libc_fs

# add include paths
LIB_DIR = $(REP_DIR)/src/lib/verilator_env
INC_DIR += $(LIB_DIR)/include \
           $(call select_from_repositories,tool/verilator/share/verilator/include)

# add include paths
vpath % $(LIB_DIR)

