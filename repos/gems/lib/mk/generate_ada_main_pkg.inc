ifeq ($(called_from_lib_mk),yes)

CUSTOM_BINDER_FLAGS ?= -n -we -D768k

SANITIZED_LIB_NAME := $(subst -,_,$(LIB))

ALIS := $(addsuffix .ali, $(basename $(SRC_ADS) $(SRC_ADB)))
ALI_DIRS += $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/ali/$(LIB)))
BINDER_SEARCH_DIRS = $(addprefix -I$(BUILD_BASE_DIR)/var/libcache/, $(LIBS)) $(addprefix -aO, $(ALI_DIRS))

# declare after(!) $(ALIS) was evaluated
SRC_ADB += b~$(SANITIZED_LIB_NAME).adb

BINDER_SRC := b~$(SANITIZED_LIB_NAME).adb

$(ALIS): message

$(BINDER_SRC): $(ALIS)
	$(VERBOSE)$(GNATBIND) $(CUSTOM_BINDER_FLAGS) $(BINDER_SEARCH_DIRS) \
	          $(INCLUDES) --RTS=$(ADA_RTS) -L$(SANITIZED_LIB_NAME)_ -o $@ $(ALIS)

all: $(BINDER_SRC)

endif
