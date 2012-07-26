TARGET = test-verilator_env-device
SRC_CC += main.cc

INC_DIR += /usr/share/verilator/include

EXT_OBJECTS += $(PRG_DIR)/verilated.o $(PRG_DIR)/Vtop__ALL.a
LIBS += server libm stdcxx libc libc_log
