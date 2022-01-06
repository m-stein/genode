TARGET   := wg_busybox
REQUIRES := x86_64

CUSTOM_TARGET_DEPS := busybox_build.phony

BUSYBOX_DIR      := $(call select_from_ports,busybox)/src/busybox
WG_TOOLS_SRC_DIR := $(call select_from_ports,wireguard-tools)/wireguard-tools/src
PWD              := $(shell pwd)
INITRAMFS_DIR    := $(PWD)/initramfs
WG_TOOLS_DST_DIR := $(PWD)/wireguard-tools

WG_1_PUBLIC_KEY  := r1Gslnm82X8NaijsWzPoSFzDZGl2tTJoPa+EJL4gYQw=
WG_1_TUNNEL_IP   := 10.0.9.1
WG_1_INTERNET_IP := 10.0.2.2
WG_1_UDP_PORT    := 55551

WG_2_PRIVATE_KEY := 8GRSQZMgG1uuvz4APIBqrDmiLj8L886r++hzixjjHFc=
WG_2_TUNNEL_IP   := 10.0.9.2
WG_2_UDP_PORT    := 55552

WG_2_PEER_PUBLIC_KEY  := $(WG_1_PUBLIC_KEY)
WG_2_PEER_ALLOWED_IPS := $(WG_1_TUNNEL_IP)/32
WG_2_PEER_ENDPOINT    := $(WG_1_INTERNET_IP):$(WG_1_UDP_PORT)

BUSYBOX_MK_ARGS = ARCH=x86_64

# filter for make output of Busybox build system
BB_BUILD_OUTPUT_FILTER = 2>&1 | sed "s/^/      [Busybox]  /"
WGT_BUILD_OUTPUT_FILTER = 2>&1 | sed "s/^/      [Wireguard Tools]  /"

busybox_config.tag:
	$(MSG_CONFIG)Busybox
	$(VERBOSE)$(MAKE) -C $(BUSYBOX_DIR) O=$(PWD) $(BUSYBOX_MK_ARGS) defconfig $(BB_BUILD_OUTPUT_FILTER)
	$(VERBOSE)sed -i "s/# CONFIG_STATIC is not set/CONFIG_STATIC=y/" .config
	$(VERBOSE)touch $@

# update Busybox config on makefile changes
busybox_config.tag: $(MAKEFILE_LIST)

busybox_build.phony: busybox_config.tag
# compile and install busybox
	$(MSG_BUILD)Busybox
	$(VERBOSE)$(MAKE) $(BUSYBOX_MK_ARGS) $(BB_BUILD_OUTPUT_FILTER)
	$(VERBOSE)$(MAKE) $(BUSYBOX_MK_ARGS) install $(BB_BUILD_OUTPUT_FILTER)
# initialize initramfs with busybox
	$(VERBOSE)rm -rf $(INITRAMFS_DIR)
	$(VERBOSE)mkdir -p $(addprefix $(INITRAMFS_DIR)/,bin sbin etc/init.d proc/net sys usr/bin usr/sbin)
	$(VERBOSE)cp -a _install/* $(INITRAMFS_DIR)
# compile wireguard tools
	$(MSG_BUILD)Wireguard Tools
	$(VERBOSE)rm -rf $(WG_TOOLS_DST_DIR)
	$(VERBOSE)cp -r $(WG_TOOLS_SRC_DIR) $(WG_TOOLS_DST_DIR)
	$(VERBOSE)$(MAKE) -C $(WG_TOOLS_DST_DIR) $(WGT_BUILD_OUTPUT_FILTER)
# install 'wg' tool and its library dependencies at initramfs
	$(VERBOSE)mkdir -p $(addprefix $(INITRAMFS_DIR)/,lib/x86_64-linux-gnu lib64)
	$(VERBOSE)cp /lib/x86_64-linux-gnu/libc.so.6 $(INITRAMFS_DIR)/lib/x86_64-linux-gnu/
	$(VERBOSE)cp /lib64/ld-linux-x86-64.so.2 $(INITRAMFS_DIR)/lib64/
	$(VERBOSE)cp $(WG_TOOLS_DST_DIR)/wg $(INITRAMFS_DIR)/bin
# add init shell script
	$(VERBOSE)echo $(WG_2_PRIVATE_KEY) > $(INITRAMFS_DIR)/wg_private_key
	$(VERBOSE)chmod 700 $(INITRAMFS_DIR)/wg_private_key
	$(VERBOSE)echo "::sysinit:/etc/init.d/rcS" > $(INITRAMFS_DIR)/etc/inittab
	$(VERBOSE)(\
	   echo "#!/bin/sh"; \
	   echo "mknod /dev/urandom c 1 9"; \
	   echo "mknod /dev/random c 1 8"; \
	   echo "echo WGSETUP 1: ip link add wg0 type wireguard"; \
	   echo "ip link add wg0 type wireguard"; \
	   echo "echo WGSETUP 2: ip addr add $(WG_2_TUNNEL_IP)/24 dev wg0"; \
	   echo "ip addr add $(WG_2_TUNNEL_IP)/24 dev wg0"; \
	   echo "echo WGSETUP 3: wg set wg0 private-key /wg_private_key listen-port $(WG_2_UDP_PORT)"; \
	   echo "wg set wg0 private-key /wg_private_key listen-port $(WG_2_UDP_PORT)"; \
	   echo "echo WGSETUP 4: ip link set wg0 up"; \
	   echo "ip link set wg0 up"; \
	   echo "echo WGSETUP 5: wg set wg0 peer $(WG_2_PEER_PUBLIC_KEY) allowed-ips $(WG_2_PEER_ALLOWED_IPS) endpoint $(WG_2_PEER_ENDPOINT)"; \
	   echo "wg set wg0 peer $(WG_2_PEER_PUBLIC_KEY) allowed-ips $(WG_2_PEER_ALLOWED_IPS) endpoint $(WG_2_PEER_ENDPOINT)"; \
	   echo "exec /bin/sh +m"\
	) > $(INITRAMFS_DIR)/etc/init.d/rcS
	$(VERBOSE)chmod 777 $(INITRAMFS_DIR)/etc/init.d/rcS
# create cpio from initramfs
	$(VERBOSE)cd $(INITRAMFS_DIR) && find . -print0 | cpio --null -ov --format=newc | gzip -9 > $(PWD)/initramfs.cpio.gz
