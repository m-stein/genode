SPECS += omap4 usb cortex_a9 tl16c750 gpio framebuffer

include $(call select_from_repositories,mk/spec-cortex_a9.mk)
include $(call select_from_repositories,mk/spec-tl16c750.mk)
