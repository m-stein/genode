MIRROR_FROM_REP_DIR := \
	include/cbe_crypto/interface.h \
	lib/mk/vfs_cbe_crypto_memcopy.mk \
	src/lib/vfs/cbe_crypto/vfs.cc \
	src/lib/vfs/cbe_crypto/memcopy

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
