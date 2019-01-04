TARGET   = fs_block
SRC_CC  += main.cc
SRC_ADB += fs_block.adb
SRC_ADB += request_buffer.adb
SRC_ADB += utils.adb
LIBS    += base ada vfs
INC_DIR += $(PRG_DIR)
