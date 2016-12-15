
SRC_CC += main.cc

LIBS += server

vpath main.cc $(REP_DIR)/src/test/sd_card_bench

include $(REP_DIR)/lib/mk/spec/imx6/sd_card.inc
