include $(REP_DIR)/lib/mk/av.inc

LIBAVDEVICE_DIR = $(call select_from_ports,libav)/src/lib/libav/libavdevice

INC_DIR += $(LIBAVDEVICE_DIR)

include $(LIBAVDEVICE_DIR)/Makefile

LIBS += avformat

vpath % $(LIBAVDEVICE_DIR)
