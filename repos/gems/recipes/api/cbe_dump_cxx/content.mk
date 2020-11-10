MIRROR_FROM_REP_DIR := \
	include/cbe/spark_object.h \
	include/cbe/types.h \
	include/cbe/dump/configuration.h \
	include/cbe/dump/library.h \
	lib/symbols/cbe_dump_cxx

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
