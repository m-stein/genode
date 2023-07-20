REQUIRES = arm_v7
TARGET   = lan9118_nic_drv
SRC_CC   = main.cc
LIBS     = base nic_driver
INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT :=
