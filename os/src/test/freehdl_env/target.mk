
# set program name
TARGET = test-freehdl_env

# add library dependencies
LIBS += freehdl_env

# add c++ source files
SRC_CC += hello.cc hello._main_.cc

# declare source paths
vpath % $(PRG_DIR)

