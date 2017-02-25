REQUIRES += nova

SRC_CC = spec/nova_x86_32/low.cc native_cpu.cc

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/spec/x86_32/gdbserver_platform-x86_32.inc
