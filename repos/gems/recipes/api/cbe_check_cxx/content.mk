MIRROR_FROM_REP_DIR := \
	include/cbe/spark_object.h \
	include/cbe/types.h \
	include/cbe/check/library.h \
	lib/symbols/cbe_check_cxx

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
