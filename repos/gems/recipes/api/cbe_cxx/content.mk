content: include/cbe lib/symbols/cbe_cxx LICENSE

include/cbe:
	mkdir -p $@
	cp -a $(REP_DIR)/include/cbe/library.h $@
	cp -a $(REP_DIR)/include/cbe/spark_object.h $@
	cp -a $(REP_DIR)/include/cbe/types.h $@

lib/symbols/cbe_cxx:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
