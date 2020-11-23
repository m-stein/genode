TARGET  = imx6_sd_card_drv
SRC_CC  = adma2.cc imx/driver.cc
INC_DIR = $(REP_DIR)/src/drivers/sd_card/imx

include $(REP_DIR)/src/drivers/sd_card/target.inc
