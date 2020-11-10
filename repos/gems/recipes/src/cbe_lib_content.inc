CBE_DIR := $(call port_dir,$(REP_DIR)/ports/cbe)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_CBE_DIR) src/lib/$(LIB_SUB_DIR)/target.mk LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_CBE_DIR):
	mkdir -p $(dir $@)
	cp -r $(CBE_DIR)/$@ $(dir $@)

src/lib/$(LIB_SUB_DIR)/target.mk: src/lib/$(LIB_SUB_DIR)
	echo "LIBS += $(LIB_SUB_DIR)" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
