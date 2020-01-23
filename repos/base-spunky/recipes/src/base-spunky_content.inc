

#######################################
## base/recipes/src/base_content.inc ##
#######################################

content: src/include src/core src/lib src/timer lib/mk LICENSE

src/include src/core src/lib src/timer lib/mk:
	mkdir -p $@
	cp -r $(GENODE_DIR)/repos/base/$@/* $@
	cp -r $(GENODE_DIR)/repos/base-hw/$@/* $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

content: etc/specs.conf

etc/specs.conf:
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/base-hw/$@ $(dir $@)


#############################################
## base-hw/recipes/src/base-hw_content.inc ##
#############################################

content: src/bootstrap

src/bootstrap:
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/base-hw/$@ $(dir $@)

content: generalize_target_names remove_other_board_libs

generalize_target_names: lib/mk src/lib src/timer
	for spec in arm riscv x86_64; do \
	  mv lib/mk/spec/$$spec/ld-hw.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-hw/ld/"           src/lib/ld/hw/target.mk
	sed -i "s/hw_timer_drv/timer/" src/timer/hw/target.mk

remove_other_board_libs: lib/mk
	find lib/mk/spec -name core-hw-*.mk -o -name bootstrap-hw-*.mk |\
		grep -v "hw-$(BOARD).mk" | xargs rm -rf


###########################################
## libports/recipes/src/spark/content.mk ##
###########################################

ADA_RT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/ada-runtime)

MIRROR_FROM_ADA_RT_DIR := \
	$(addprefix ada-runtime/contrib/gcc-8.3.0/,\
		ada.ads \
		a-unccon.ads \
		interfac.ads \
		i-cexten.ads \
		s-maccod.ads \
		s-unstyp.ads \
	) \
	$(addprefix ada-runtime/src/minimal/,\
		a-except.ads \
		a-except.adb \
		i-c.ads \
		i-c.adb \
		system.ads \
		s-arit64.ads \
		s-arit64.adb \
		s-exctab.ads \
		s-exctab.adb \
		s-parame.ads \
		s-parame.adb \
		s-secsta.ads \
		s-secsta.adb \
		s-soflin.ads \
		s-soflin.adb \
		s-stalib.ads \
		s-stalib.adb \
		s-stoele.ads \
		s-stoele.adb \
	) \
	$(addprefix ada-runtime/src/lib/,\
		componolit.ads \
		componolit-runtime.ads \
		componolit-runtime-conversions.ads \
		componolit-runtime-conversions.adb \
		componolit-runtime-debug.ads \
		componolit-runtime-debug.adb \
		componolit-runtime-exceptions.ads \
		componolit-runtime-platform.ads \
		componolit-runtime-platform.adb \
		componolit-runtime-strings.ads \
		componolit-runtime-strings.adb \
		init.c \
	) \
	ada-runtime/src/common/s-init.ads \
	ada-runtime/src/common/s-init.adb \
	ada-runtime/platform/genode/genode.cc \
	ada-runtime/platform/componolit_runtime.h \
	ada-runtime/platform/ada_exceptions.h \
	ada-runtime/platform/gnat_helpers.h

content: $(MIRROR_FROM_ADA_RT_DIR)

$(MIRROR_FROM_ADA_RT_DIR):
	mkdir -p $(dir $@)
	cp -r $(ADA_RT_DIR)/$@ $@

MIRROR_FROM_LIBPORTS := \
	lib/mk/spark.mk \
	lib/mk/spark.inc \
	lib/import/import-spark.mk \
	include/ada/exception.h

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)



######################
## spunky adaptions ##
######################

content: spunky_adaptions

spunky_adaptions:
	cp -rf $(REP_DIR)/lib/mk/core-spunky.inc lib/mk/
	cp -rf $(REP_DIR)/lib/mk/spec/x86_64/core-spunky-pc.mk lib/mk/spec/x86_64/
	cp -rf $(REP_DIR)/src/core/ada src/core/
	cp -rf $(REP_DIR)/src/core/hw/target.mk src/core/hw/
	cp -rf $(REP_DIR)/src/core/kernel/* src/core/kernel/
	rm -rf lib/mk/spec/arm
	rm -rf lib/mk/spec/muen
	rm -rf lib/mk/spec/riscv
	rm -rf lib/mk/spec/arm_64
	rm -rf lib/mk/spec/arm_v8
	rm -rf lib/mk/spec/x86_64/core-hw-pc.mk
