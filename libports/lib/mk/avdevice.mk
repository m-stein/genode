include $(REP_DIR)/lib/mk/av.inc

include $(REP_DIR)/lib/import/import-avdevice.mk

LIBAVDEVICE_DIR = $(REP_DIR)/contrib/$(LIBAV)/libavdevice

include $(LIBAVDEVICE_DIR)/Makefile

vpath % $(LIBAVDEVICE_DIR)
