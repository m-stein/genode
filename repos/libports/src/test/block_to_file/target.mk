TARGET  = block_to_file
SRC_CC  = main.cc
SRC_ADB += app-block.adb
SRC_ADB += app-packet_buffer.adb
SRC_ADB += app.adb
LIBS    = base ada vfs
INC_DIR += $(PRG_DIR)
