MIRROR_FROM_REP_DIR := \
	include/cbe/spark_object.h \
	include/cbe/types.h \
	include/cbe_init/configuration.h \
	include/cbe_init/library.h \
	lib/symbols/cbe_init_cxx

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
