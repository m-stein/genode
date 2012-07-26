#
# \brief  Test joint of a freehdl emulator and vinit
# \author Martin Stein
# \date   2012-05-04
#

# set program name
TARGET = test-freehdl_joint-adder

# add C++ sources
SRC_CC += adder._main_.cc adder.cc emulation_frontend.cc

# add include paths
INC_DIR += $(PRG_DIR)

# add library dependencies
LIBS += cxx env server freehdl_env

# declare source paths
vpath % $(PRG_DIR)

