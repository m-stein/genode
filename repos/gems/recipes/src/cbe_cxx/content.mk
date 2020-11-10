LIB_SUB_DIR := cbe_cxx

MIRROR_FROM_CBE_DIR := \
	src/lib/cbe \
	src/lib/cbe_common \
	src/lib/cbe_cxx \
	src/lib/cbe_cxx_common \
	src/lib/sha256_4k

MIRROR_FROM_REP_DIR := \
	lib/import/import-cbe.mk \
	lib/import/import-cbe_common.mk \
	lib/import/import-sha256_4k.mk \
	lib/mk/cbe.mk \
	lib/mk/cbe_common.mk \
	lib/mk/cbe_cxx.mk \
	lib/mk/cbe_cxx_common.mk \
	lib/mk/generate_ada_main_pkg.inc \
	lib/mk/sha256_4k.mk

include $(REP_DIR)/recipes/src/cbe_lib_content.inc
