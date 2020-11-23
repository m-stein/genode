LIB_SUB_DIR := cbe_init_cxx

MIRROR_FROM_CBE_DIR := \
	src/lib/cbe \
	src/lib/cbe_common \
	src/lib/cbe_cxx \
	src/lib/cbe_cxx_common \
	src/lib/cbe_init \
	src/lib/cbe_init_cxx \
	src/lib/sha256_4k

MIRROR_FROM_REP_DIR := \
	lib/import/import-cbe.mk \
	lib/import/import-cbe_common.mk \
	lib/import/import-cbe_init.mk \
	lib/import/import-sha256_4k.mk \
	lib/mk/spec/x86_64/cbe.mk \
	lib/mk/spec/x86_64/cbe_common.mk \
	lib/mk/spec/x86_64/cbe_cxx.mk \
	lib/mk/spec/x86_64/cbe_cxx_common.mk \
	lib/mk/spec/x86_64/cbe_init.mk \
	lib/mk/spec/x86_64/cbe_init_cxx.mk \
	lib/mk/generate_ada_main_pkg.inc \
	lib/mk/sha256_4k.mk

include $(REP_DIR)/recipes/src/cbe_lib_content.inc
