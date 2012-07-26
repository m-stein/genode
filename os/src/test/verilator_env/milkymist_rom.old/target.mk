
TARGET = test-vre-milkymist_rom

SRC_CC += wishbone_slave.cc

EXT_OBJECTS += $(PRG_DIR)/verilated.o $(PRG_DIR)/Vtop__ALL.a

LIBS += verilator_env_wb
