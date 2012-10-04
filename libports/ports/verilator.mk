#
# \brief   Make-prepare backend for the package 'verilator'
# \author  Martin Stein
# \date    2012-08-03
#


VERILATOR_REV   = verilator-3.840
VERILATOR_TGZ   = $(VERILATOR_REV).tgz
VERILATOR_URL   = http://www.veripool.org/ftp/$(VERILATOR_TGZ)

# we need absolute paths for these two to instruct 'configure'
VERILATOR_SRC   = $(realpath $(CONTRIB_DIR))/$(VERILATOR_REV)

VERILATOR_BUILD   = $(realpath ./)/tool/verilator
VERILATOR_CONF    = $(VERILATOR_SRC)/configure
VERILATOR_MAKE    = $(VERILATOR_BUILD)/Makefile
VERILATOR_BIN     = $(VERILATOR_BUILD)/bin/verilator
VERILATOR_PATCHES = src/verilator-pkg


#####################################
## Interface to top-level Makefile ##
#####################################

PORTS += $(VERILATOR_REV)

prepare-verilator: $(VERILATOR_BIN)

$(VERILATOR_SRC): clean-verilator


###############################
## Port-specific local rules ##
###############################

$(VERILATOR_BUILD):
	mkdir -p $@

$(VERILATOR_MAKE): $(VERILATOR_SRC) $(VERILATOR_BUILD)
	patch -d $(VERILATOR_SRC) -N -p1 < $(VERILATOR_PATCHES)/verilator.patch
	patch -d $(VERILATOR_SRC) -N -p1 < $(VERILATOR_PATCHES)/emulators_build_system.patch
	cd $(VERILATOR_SRC); unset VERILATOR_ROOT; $(VERILATOR_CONF) --prefix $(VERILATOR_BUILD)

$(VERILATOR_BIN): $(VERILATOR_MAKE)
	$(call check_tool,flex)
	$(call check_tool,bison)
	cd $(VERILATOR_SRC); unset VERILATOR_ROOT; make -j4; make install

$(DOWNLOAD_DIR)/$(VERILATOR_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(VERILATOR_URL) && touch $@

$(VERILATOR_SRC): $(DOWNLOAD_DIR)/$(VERILATOR_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@

clean-verilator:
	$(VERBOSE)rm -rf $(VERILATOR_SRC)
