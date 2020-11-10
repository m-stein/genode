MIRROR_FROM_REP_DIR := \
	lib/mk/vfs_cbe.mk \
	include/util \
	src/lib/vfs/cbe

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
