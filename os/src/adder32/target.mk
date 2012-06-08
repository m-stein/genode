#
# \brief  32-bit-adder-emulator
# \author Martin Stein
# \date   2012-05-04
#

# Set program name
TARGET = adder32

# Add C++ sources
SRC_CC += main.cc

# Add libraries
LIBS += cxx env server

vpath main.cc $(PRG_DIR)

