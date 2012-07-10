#
# \brief   Generic parts of an 'init' that enables device emulation
# \author  Martin Stein
# \date    2012-04-27
#

# set program name
TARGET = vinit

# add C++ sources
SRC_CC += main.cc

# add library dependencies
LIBS += env cxx server process signal

# add include paths
INC_DIR += $(GENERIC_PRG_DIR)/include/

# declare source paths
vpath % $(GENERIC_PRG_DIR)

