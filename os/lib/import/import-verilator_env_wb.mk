
# add include paths
REP_INC_DIR += src/lib/verilator_env/wishbone

# include imports of implied libraries
include $(call select_from_repositories,lib/import/import-verilator_env.mk)

