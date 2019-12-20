BOARD    ?= unknown
TARGET   := core_spunky_$(BOARD)
LIBS     := core-spunky-$(BOARD)
CORE_OBJ := core-spunky-$(BOARD).o

include $(BASE_DIR)/src/core/target.inc
