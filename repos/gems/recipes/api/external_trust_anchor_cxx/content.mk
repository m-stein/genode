content: include/cbe lib/symbols/external_trust_anchor_cxx LICENSE

include/cbe:
	mkdir -p $@
	cp -a $(REP_DIR)/include/cbe/external_ta.h $@
	cp -a $(REP_DIR)/include/cbe/spark_object.h $@
	cp -a $(REP_DIR)/include/cbe/types.h $@

lib/symbols/external_trust_anchor_cxx:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
