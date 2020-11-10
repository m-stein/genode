MIRROR_FROM_REP_DIR := \
	include/cbe/vfs/trust_anchor_vfs.h \
	include/cbe/vfs/io_job.h \
	src/app/cbe_init

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
