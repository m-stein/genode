TARGET       = mini_kernel
STARTUP_LIB  =
SRC_CC       = main.cc
SRC_S        = crt0.s exc_vector.s
CC_OPT       = -fno-exceptions -fno-rtti
LD_TEXT_ADDR = 0x60000000
