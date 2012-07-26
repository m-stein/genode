#
# \brief   Emulation enviroment for verilated wishbone-slaves
# \author  Martin Stein
# \date    2012-07-15
#

# add library dependencies
LIBS += verilator_env

# add C++ source files
SRC_CC += emulation_session_component.cc

# declare source paths
vpath % $(REP_DIR)/src/lib/verilator_env/wishbone

