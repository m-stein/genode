
# add include paths
REP_INC_DIR += src/lib/verilator_env/include
INC_DIR += /usr/share/verilator/include

# include imports of implied libraries
include $(call select_from_repositories,lib/import/import-libc.mk)
include $(call select_from_repositories,lib/import/import-stdcxx.mk)

