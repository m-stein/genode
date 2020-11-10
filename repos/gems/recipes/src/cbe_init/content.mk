MIRROR_FROM_REP_DIR := \
	include/util \
	src/app/cbe_init \

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(REP_DIR)/LICENSE $@
