TARGET = nic_uplink

LIBS += base net

SRC_CC += main.cc
SRC_CC += communication_buffer.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/server/nic_router

vpath communication_buffer.cc $(REP_DIR)/src/server/nic_router
