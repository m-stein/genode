#
# \brief  FPU emulator
# \author Martin Stein
# \date   2012-05-04
#

# set program name
TARGET = test-vinit-fpu

# add C++ sources
SRC_CC += main.cc

# add include paths
INC_DIR += $(PRG_DIR)

# add library dependencies
LIBS += cxx env server signal

# declare source paths
vpath main.cc $(PRG_DIR)

