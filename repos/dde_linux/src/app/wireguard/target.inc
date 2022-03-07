TARGET    = wireguard
LIBS     += base net jitterentropy lx_emul
GEN_DIR   = $(PRG_DIR)/../..
INC_DIR  += $(GEN_DIR)
SRC_C    += $(notdir $(wildcard $(GEN_DIR)/generated_dummies.c))
SRC_C    += dummies.c
SRC_C    += lx_emul.c
SRC_C    += wireguard.c
SRC_CC   += arp_cache.cc
SRC_CC   += arp_waiter.cc
SRC_CC   += base64.cc
SRC_CC   += config_model.cc
SRC_CC   += dhcp_client.cc
SRC_CC   += ipv4_address_prefix.cc
SRC_CC   += ipv4_config.cc
SRC_CC   += irq.cc
SRC_CC   += main.cc
SRC_CC   += nic_connection.cc
SRC_CC   += random.cc
SRC_CC   += uplink_connection.cc

CC_OPT_wireguard += -DKBUILD_MODFILE='"wireguard"' \
                    -DKBUILD_BASENAME='"wireguard"' \
                    -DKBUILD_MODNAME='"wireguard"'
CC_OPT_lx_emul   += -DKBUILD_MODFILE='"lx_emul"' \
                    -DKBUILD_BASENAME='"lx_emul"' \
                    -DKBUILD_MODNAME='"lx_emul"'

vpath wireguard.c $(GEN_DIR)/genode_c_api
vpath %.c  $(GEN_DIR)
vpath %.cc $(GEN_DIR)

CC_CXX_WARN_STRICT_CONVERSION =
