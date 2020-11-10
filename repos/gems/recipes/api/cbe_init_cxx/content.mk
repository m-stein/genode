MIRROR_FROM_REP_DIR := \
	include/cbe/spark_object.h \
	include/cbe/types.h \
	include/cbe/init/configuration.h \
	include/cbe/init/library.h \
	lib/symbols/cbe_init_cxx

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
