REQUIRES += foc

SRC_CC = spec/foc_x86_32/low.cc \
         spec/foc/native_cpu.cc

LIBS += syscall-foc

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/spec/x86_32/gdbserver_platform-x86_32.inc
