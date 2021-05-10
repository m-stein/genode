INC_DIR += $(BASE_DIR)/../base-hw/src/include
INC_DIR += $(BASE_DIR)/../base-hw/include
INC_DIR += $(BASE_DIR)/../base-hw/include/spec/x86_64

vpath %.cc $(BASE_DIR)/../base-hw/src/lib/base

include $(BASE_DIR)/../base-hw/lib/mk/base-hw-common.mk
