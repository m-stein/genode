TARGET := system_rtc
SRC_CC := main.cc
LIBS   := base

# musl-libc contrib sources
SRC_C  := secs_to_tm.c tm_to_secs.c
INC_DIR := $(PRG_DIR)/contrib
vpath %.c $(PRG_DIR)/contrib
