MIRROR_FROM_REP_DIR := \
	lib/mk/spec/x86_64/wireguard.mk \
	lib/mk/lx_emul.mk \
	lib/import/import-lx_emul.mk \
	lib/import/import-lx_emul_common.inc \
	lib/mk/spec/x86_64/virt_linux_generated.mk \
	lib/mk/virt_linux_generated.inc \
	src/virt_linux \
	src/app/wireguard \
	src/lib/lx_emul \
	src/lib/lx_kit \
	src/include/spec/x86_64 \
	src/include/spec/x86 \
	src/include/lx_emul \
	src/include/lx_kit \
	src/include/lx_user

MIRROR_FROM_PORT_DIR := src/linux

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/linux)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
