
# add include paths
INC_DIR += $(call select_from_repositories,contrib/freehdl-0.0.7)

# include implied imports
include $(call select_from_repositories,lib/import/import-libc.mk)
include $(call select_from_repositories,lib/import/import-stdcxx.mk)

