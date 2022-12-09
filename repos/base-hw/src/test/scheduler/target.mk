TARGET := test-scheduler

SRC_CC += main.cc
SRC_CC += scheduler.cc

INC_DIR += $(REP_DIR)/src/core
INC_DIR += $(REP_DIR)/src/include
INC_DIR += $(BASE_DIR)/src/include

LIBS += base

vpath main.cc      $(PRG_DIR)
vpath scheduler.cc $(REP_DIR)/src/core/kernel
