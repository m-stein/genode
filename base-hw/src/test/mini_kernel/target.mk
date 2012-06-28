TARGET      = mini_kernel
STARTUP_LIB =
SRC_CC      = main.cc
SRC_S       = crt0.s exc_vector.s
CC_OPT      = -fno-exceptions -fno-rtti

vpath crt0.s $(REP_DIR)/src/platform