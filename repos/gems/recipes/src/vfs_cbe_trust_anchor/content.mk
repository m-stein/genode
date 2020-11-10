MIRROR_FROM_REP_DIR := \
	lib/mk/vfs_cbe_trust_anchor.mk \
	include/util/io_job.h \
	src/lib/vfs/cbe_trust_anchor

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
