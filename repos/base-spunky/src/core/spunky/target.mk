BOARD    ?= unknown
TARGET   := core_spunky_$(BOARD)
LIBS     := core-spunky-$(BOARD)
CORE_LIB := core-spunky-$(BOARD).a

include $(BASE_DIR)/src/core/target.inc
