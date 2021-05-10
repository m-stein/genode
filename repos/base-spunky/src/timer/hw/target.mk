TARGET   = hw_timer_drv
LIBS     = syscall-hw
INC_DIR += $(BASE_DIR)/../base-hw/src/timer/hw
INC_DIR += $(BASE_DIR)/../base-hw/include
INC_DIR += $(BASE_DIR)/../base-hw/include/spec/x86_64
SRC_CC  += time_source.cc

SRC_CC  += main.cc
LIBS    += base
INC_DIR += $(call select_from_repositories,src/timer/include)

vpath main.cc $(dir $(call select_from_repositories,src/timer/main.cc))
vpath time_source.cc $(BASE_DIR)/../base-hw/src/timer/hw
