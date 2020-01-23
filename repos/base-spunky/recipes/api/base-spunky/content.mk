FROM_BASE_HW := etc include

content: $(FROM_BASE_HW) LICENSE

$(FROM_BASE_HW):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/base-hw/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
