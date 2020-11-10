MIRROR_FROM_REP_DIR := \
	include/cbe/types.h \
	include/cbe_crypto/interface.h \
	lib/mk/vfs_cbe_crypto_aes_cbc.mk \
	src/lib/vfs/cbe_crypto/vfs.cc \
	src/lib/vfs/cbe_crypto/aes_cbc

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
