MIRROR_FROM_REP_DIR := \
	lib/mk/vfs_cbe.mk \
	include/cbe/vfs/trust_anchor_vfs.h \
	include/cbe/vfs/io_job.h \
	src/lib/vfs/cbe

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
