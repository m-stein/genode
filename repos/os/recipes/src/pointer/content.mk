SRC_DIR := src/app/pointer

include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: $(MIRROR_FROM_REP_DIR) include/report_rom include/pointer

$(MIRROR_FROM_REP_DIR) include/report_rom include/pointer:
	$(mirror_from_rep_dir)
