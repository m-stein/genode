SRC_C   += $(addprefix net/usb/, usbnet.c asix_devices.c asix_common.c ax88172a.c \
             ax88179_178a.c)
SRC_C   += usb/host/ehci-exynos.c

include $(REP_DIR)/lib/mk/xhci.inc
include $(REP_DIR)/lib/mk/usb.inc
include $(REP_DIR)/lib/mk/armv7/usb.inc

CC_OPT  += -DCONFIG_USB_EHCI_TT_NEWSCHED -DCONFIG_USB_DWC3_HOST=1 \
           -DCONFIG_USB_DWC3_GADGET=0 -DCONFIG_USB_OTG_UTILS -DCONFIG_USB_XHCI_PLATFORM -DDWC3_QUIRK
SRC_CC  += platform.cc

#DWC3
SRC_C   += $(addprefix usb/dwc3/, host.c core.c)

#XHCI
SRC_C   += usb/host/xhci-plat.c

vpath platform.cc $(LIB_DIR)/arm/exynos5
