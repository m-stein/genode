content: init.config assets.tar

init.config:
	cp $(REP_DIR)/recipes/raw/download_debian/$@ $@

assets.tar:
	tar --mtime='2018-05-29 00:00Z' -cf $@ -C $(REP_DIR)/run/sculpt machine.vbox
	gzip -cdfk $(REP_DIR)/run/sculpt/machine.vdi.gz > ./machine.vdi
	tar --mtime='2018-05-29 00:00Z' -rf $@ ./machine.vdi
	rm ./machine.vdi
