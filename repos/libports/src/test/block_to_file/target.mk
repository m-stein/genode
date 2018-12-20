TARGET  = block_to_file
SRC_CC  = main.cc
SRC_ADB += global-block.adb
SRC_ADB += global-packet_buffer.adb
SRC_ADB += global.adb
LIBS    = base ada vfs
INC_DIR += $(PRG_DIR)
