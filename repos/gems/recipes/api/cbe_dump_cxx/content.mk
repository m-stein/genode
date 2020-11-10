MIRROR_FROM_REP_DIR := \
	include/cbe/spark_object.h \
	include/cbe/types.h \
	include/cbe_dump/configuration.h \
	include/cbe_dump/library.h \
	lib/symbols/cbe_dump_cxx

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
