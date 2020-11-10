####################
# Public interface #
####################

DEPOT_USER ?= cbe_autopilot
GENODE_DIR ?= $(shell pwd)/../../..
CBE_REPO_DIR ?= $(shell pwd)/..
BUILD_DIR ?= $(GENODE_DIR)/build/.cbe_autopilot
CONTRIB_DIR ?= $(GENODE_DIR)/.contrib.$(DEPOT_USER)
DEPOT_DIR ?= $(GENODE_DIR)/.depot.$(DEPOT_USER)
VERBOSE ?= @
JOBS ?= $(shell nproc)
CROSS_DEV_PREFIX ?= genode-x86-
ARCH ?= x86_64


##############
# File paths #
##############

BUILD_CONF := $(BUILD_DIR)/etc/build.conf
TOOLS_CONF := $(BUILD_DIR)/etc/tools.conf
CBE_CI_DIR := $(CBE_REPO_DIR)/.ci


#########
# Tools #
#########

BUILD_DIR_MAKE := \
   CROSS_DEV_PREFIX=$(CROSS_DEV_PREFIX) \
   CONTRIB_DIR=$(CONTRIB_DIR) \
   DEPOT_DIR=$(DEPOT_DIR) \
   KERNEL=linux \
   BOARD=linux \
   make -C $(BUILD_DIR)

PREPARE_PORTS := \
   CROSS_DEV_PREFIX=$(CROSS_DEV_PREFIX) \
   CONTRIB_DIR=$(CONTRIB_DIR) \
   $(GENODE_DIR)/tool/ports/prepare_port -j$(JOBS)

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

PORTS := \
   ada-runtime bash coreutils curl dde_linux libc libiconv libsparkcrypto \
   libssh ncurses nova openssl qemu-usb stdcxx virtualbox5 zlib stb \
   ttf-bitstream-vera

TARGETS := \
   $(shell find $(CBE_REPO_DIR)/src -name target.mk | \
              sed "s/^.*\/src\///" | \
              sed "s/\/target\.mk$$//")

LIBS := \
   $(shell find $(CBE_REPO_DIR)/lib/mk -name *.mk | \
              sed "s/^.*\///" | \
              sed "s/\.mk$$//")

TEST_NAMES := \
   $(shell find $(CBE_REPO_DIR)/run -name *.run | \
              sed "s/^.*\///" | \
              sed "s/\.run$$//")

TESTS := $(addprefix run/,$(TEST_NAMES))

PACKAGE_NAMES := \
   $(shell find $(CBE_REPO_DIR)/recipes/pkg -name hash | \
              sed "s/^.*\/pkg\///" | \
              sed "s/\/hash$$//")

PACKAGES := $(addprefix $(DEPOT_USER)/pkg/$(ARCH)/,$(PACKAGE_NAMES))

RUN_OPT_DAU := RUN_OPT += --depot-auto-update

.PHONY: ports
ports:
	$(VERBOSE)$(PREPARE_PORTS) $(PORTS)

.PHONY: build_dir
build_dir:
	$(VERBOSE)$(CREATE_BUILD_DIR) $(ARCH)
	$(VERBOSE)sed -i 's/^#REPOSITORIES +=/REPOSITORIES +=/g' $(BUILD_CONF)
	$(VERBOSE)sed -i 's/^CONTRIB_DIR :=.*$$//g' $(BUILD_CONF)
	$(VERBOSE)sed -i 's/^#MAKE += -j4$$/MAKE += -j$(JOBS)/g' $(BUILD_CONF)
	$(VERBOSE)sed -i 's/^#$(RUN_OPT_DAU)$$/$(RUN_OPT_DAU)/g' $(BUILD_CONF)
	$(VERBOSE)echo 'REPOSITORIES += $$(GENODE_DIR)/repos/cbe' >> $(BUILD_CONF)
	$(VERBOSE)echo 'RUN_OPT += --autopilot' >> $(BUILD_CONF)
	$(VERBOSE)echo 'RUN_OPT += --depot-user $(DEPOT_USER)' >> $(BUILD_CONF)
	$(VERBOSE)echo 'CROSS_DEV_PREFIX=$(CROSS_DEV_PREFIX)' > $(TOOLS_CONF)

.PHONY: prove
prove:
	$(VERBOSE)git -C $(CBE_REPO_DIR) submodule init
	$(VERBOSE)git -C $(CBE_REPO_DIR) submodule update
	$(VERBOSE)make -C $(CBE_CI_DIR)/lsc \
		NO_SPARK=1 DESTDIR=$(CBE_CI_DIR)/out/lsc install
	$(VERBOSE)$(GNAT_PROVE) $(CBE_CI_DIR)/cbe.gpr

.PHONY: packages
packages:
	$(VERBOSE)$(DEPOT_CREATE) $(PACKAGES)

.PHONY: targets
targets:
	$(VERBOSE)$(BUILD_DIR_MAKE) $(TARGETS)

.PHONY: libs
libs: $(LIBS)

.PHONY: $(LIBS)
$(LIBS):
	$(VERBOSE)$(BUILD_DIR_MAKE) LIB=$@

.PHONY: basics
basics: prove
	$(VERBOSE)$(DEPOT_CREATE) $(DEPOT_USER)/pkg/x86_64/cbe_demo
	$(VERBOSE)$(BUILD_DIR_MAKE) run/cbe_tester
	$(VERBOSE)$(BUILD_DIR_MAKE) run/vfs_cbe_init
	$(VERBOSE)$(BUILD_DIR_MAKE) run/vfs_cbe

.PHONY: tests
tests:
	$(VERBOSE)$(BUILD_DIR_MAKE) $(TESTS)

.PHONY: clean
clean:
	$(VERBOSE)rm -rf $(BUILD_DIR) $(DEPOT_DIR) $(CONTRIB_DIR)
