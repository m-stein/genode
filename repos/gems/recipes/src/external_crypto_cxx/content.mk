MIRROR_FROM_REP_DIR := \
	src/lib/cbe \
	src/lib/cbe_common \
	src/lib/cbe_cxx \
	src/lib/cbe_cxx_common \
	src/lib/external \
	src/lib/external_crypto \
	src/lib/external_crypto_cxx \
	src/lib/sha256_4k \
	lib/import/import-cbe.mk \
	lib/import/import-cbe_common.mk \
	lib/import/import-external.mk \
	lib/import/import-external_crypto.mk \
	lib/import/import-sha256_4k.mk \
	lib/mk/cbe.mk \
	lib/mk/cbe_common.mk \
	lib/mk/cbe_cxx.mk \
	lib/mk/cbe_cxx_common.mk \
	lib/mk/external.mk \
	lib/mk/external_crypto.mk \
	lib/mk/external_crypto_cxx.mk \
	lib/mk/generate_ada_main_pkg.inc \
	lib/mk/sha256_4k.mk

content: $(MIRROR_FROM_REP_DIR) \
	src/lib/external_crypto_cxx/target.mk \
	LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/external_crypto_cxx/target.mk: src/lib/external_crypto_cxx
	echo "LIBS += external_crypto_cxx" > $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
