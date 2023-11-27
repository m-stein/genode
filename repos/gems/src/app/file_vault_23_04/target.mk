TARGET  := file_vault_23_04
SRC_CC  += main.cc menu_view_dialog.cc capacity.cc
INC_DIR += $(PRG_DIR)
LIBS    += base sandbox vfs

CC_OPT += -Os
