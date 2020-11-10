####################
# Public interface #
####################

GENODE_DIR ?= $(shell pwd)/../..
BUILD_DIR ?= $(GENODE_DIR)/.build.cbe_autopilot
DEPOT_USER ?= cbe_autopilot
DEPOT_DIR ?= $(GENODE_DIR)/.depot.cbe_autopilot
VERBOSE ?= @
JOBS ?= $(shell nproc)
CROSS_DEV_PREFIX ?= genode-x86-


##########################
# Internal configuration #
##########################

ARCH := x86_64
CONTRIB_DIR := $(GENODE_DIR)/contrib
BUILD_CONF := $(BUILD_DIR)/etc/build.conf
TOOLS_CONF := $(BUILD_DIR)/etc/tools.conf
CBE_PROJECT_FILE := $(BUILD_DIR)/cbe.gpr
LSC_INSTALL_DIR := $(BUILD_DIR)/libsparkcrypto_install


####################
# Port directories #
####################

REPOSITORIES += $(GENODE_DIR)/repos/base
REPOSITORIES += $(GENODE_DIR)/repos/base-nova
REPOSITORIES += $(GENODE_DIR)/repos/base-linux
REPOSITORIES += $(GENODE_DIR)/repos/os
REPOSITORIES += $(GENODE_DIR)/repos/libports
REPOSITORIES += $(GENODE_DIR)/repos/ports
REPOSITORIES += $(GENODE_DIR)/repos/dde_linux
REPOSITORIES += $(GENODE_DIR)/repos/gems

include $(GENODE_DIR)/repos/base/mk/util.inc

PORTS := \
   ada-runtime bash coreutils curl dde_linux libc libiconv libsparkcrypto \
   libssh ncurses nova openssl qemu-usb stdcxx virtualbox5 zlib stb \
   ttf-bitstream-vera cbe

MISSING_PORTS := \
	$(strip $(foreach PORT,$(PORTS),$(if $(call select_from_ports,$(PORT)),,$(PORT))))

ifneq ($(MISSING_PORTS),)
checked_ports_exist:
	@echo ""
	@echo "Error: Ports not prepared or outdated:"
	@echo "  $(MISSING_PORTS)"
	@echo ""
	@echo "You can prepare respectively update them as follows:"
	@echo "  $(GENODE_DIR)/tool/ports/prepare_port $(MISSING_PORTS) -j$(JOBS)"
	@echo ""
	@false
else
checked_ports_exist:
endif

CBE_DIR := $(call select_from_ports,cbe)/cbe
LSC_DIR := $(call select_from_ports,libsparkcrypto)/libsparkcrypto


############
# Commands #
############

BUILD_DIR_MAKE := \
   CROSS_DEV_PREFIX=$(CROSS_DEV_PREFIX) \
   CONTRIB_DIR=$(CONTRIB_DIR) \
   DEPOT_DIR=$(DEPOT_DIR) \
   KERNEL=linux \
   BOARD=linux \
   make -sC $(BUILD_DIR)

DEPOT_CREATE := \
   CROSS_DEV_PREFIX=$(CROSS_DEV_PREFIX) \
   CONTRIB_DIR=$(CONTRIB_DIR) \
   DEPOT_DIR=$(DEPOT_DIR) \
   UPDATE_VERSIONS=1 \
   FORCE=1 \
   REBUILD= \
   $(GENODE_DIR)/tool/depot/create -j$(JOBS)

CREATE_BUILD_DIR := \
   BUILD_DIR=$(BUILD_DIR) \
   $(GENODE_DIR)/tool/create_builddir

GNAT_PROVE_FLAGS := \
   --mode=flow \
   -j0 \
   --prover=z3,cvc4 \
   --steps=1000 \
   --memlimit=1000 \
   --checks-as-errors \
   --warnings=error \
   -U \
   -P

GNAT_PROVE := gnatprove $(GNAT_PROVE_FLAGS)


###########
# Targets #
###########

RUN_OPT_DAU := RUN_OPT += --depot-auto-update

$(BUILD_DIR):
	$(VERBOSE)$(CREATE_BUILD_DIR) $(ARCH)
	$(VERBOSE)sed -i 's/^#REPOSITORIES +=/REPOSITORIES +=/g' $(BUILD_CONF)
	$(VERBOSE)sed -i 's/^CONTRIB_DIR :=.*$$//g' $(BUILD_CONF)
	$(VERBOSE)sed -i 's/^#MAKE += -j4$$/MAKE += -j$(JOBS)/g' $(BUILD_CONF)
	$(VERBOSE)sed -i 's/^#$(RUN_OPT_DAU)$$/$(RUN_OPT_DAU)/g' $(BUILD_CONF)
	$(VERBOSE)echo 'REPOSITORIES += $$(GENODE_DIR)/repos/cbe' >> $(BUILD_CONF)
	$(VERBOSE)echo 'RUN_OPT += --autopilot' >> $(BUILD_CONF)
	$(VERBOSE)echo 'RUN_OPT += --depot-user $(DEPOT_USER)' >> $(BUILD_CONF)
	$(VERBOSE)echo 'CROSS_DEV_PREFIX=$(CROSS_DEV_PREFIX)' > $(TOOLS_CONF)

$(CBE_PROJECT_FILE): $(BUILD_DIR) checked_ports_exist
	$(VERBOSE)echo 'with "$(LSC_INSTALL_DIR)/libsparkcrypto";' > $(CBE_PROJECT_FILE)
	$(VERBOSE)echo 'project CBE is' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   for Create_Missing_Dirs use "True";' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   for Object_Dir use "$(BUILD_DIR)/cbe_gpr_object_dir";' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   for Source_Dirs use ("$(CBE_DIR)/src/lib/sha256_4k", "$(CBE_DIR)/src/lib/cbe", "$(CBE_DIR)/src/lib/cbe_common");' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   for Library_Kind use "static";' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   for Library_Name use "cbe";' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   --  show full paths' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   package Compiler is' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '      for Switches ("Ada") use ("-gnatef");' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo '   end Compiler;' >> $(CBE_PROJECT_FILE)
	$(VERBOSE)echo 'end CBE;' >> $(CBE_PROJECT_FILE)

$(LSC_INSTALL_DIR): $(BUILD_DIR) checked_ports_exist
	$(VERBOSE)make -sC $(LSC_DIR) NO_SPARK=1 DESTDIR=$(LSC_INSTALL_DIR) install

.PHONY: prove
prove: $(LSC_INSTALL_DIR) $(CBE_PROJECT_FILE) checked_ports_exist
	$(VERBOSE)$(GNAT_PROVE) $(CBE_PROJECT_FILE)

.PHONY: basics
basics: $(BUILD_DIR) prove checked_ports_exist
	$(VERBOSE)$(DEPOT_CREATE) $(DEPOT_USER)/pkg/x86_64/cbe_demo
	$(VERBOSE)$(BUILD_DIR_MAKE) run/cbe_tester
	$(VERBOSE)$(BUILD_DIR_MAKE) run/vfs_cbe_init
	$(VERBOSE)$(BUILD_DIR_MAKE) run/vfs_cbe

.PHONY: clean
clean:
	$(VERBOSE)rm -rf $(BUILD_DIR) $(DEPOT_DIR)
