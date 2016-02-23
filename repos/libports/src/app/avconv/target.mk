include $(REP_DIR)/lib/import/import-av.inc

TARGET = avconv
SRC_C  = avconv.c cmdutils.c
LIBS  += avfilter avformat avcodec avutil avdevice swscale

LIBS  += libc libm config_args

LIBAV_PORT_DIR := $(call select_from_ports,libav)

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/lib/libav

CC_WARN += -Wno-parentheses -Wno-switch -Wno-uninitialized \
           -Wno-format-zero-length -Wno-pointer-sign

CC_C_OPT += -DLIBAV_VERSION=\"0.8\"

vpath %.c $(LIBAV_PORT_DIR)/src/lib/libav
