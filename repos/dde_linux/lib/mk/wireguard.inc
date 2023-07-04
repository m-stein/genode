LIBS += base net jitterentropy wireguard_lx_inc_dirs

INC_DIR += $(REP_DIR)/src/app/wireguard
INC_DIR += $(REP_DIR)/src/include/virt_linux
INC_DIR += $(REP_DIR)/src/include

SRC_CC += arp_cache.cc
SRC_CC += arp_waiter.cc
SRC_CC += base64.cc
SRC_CC += config_model.cc
SRC_CC += dhcp_client.cc
SRC_CC += ipv4_address_prefix.cc
SRC_CC += ipv4_config.cc
SRC_CC += lx_emul/random.cc
SRC_CC += main.cc
SRC_CC += nic_connection.cc
SRC_CC += uplink_connection.cc

vpath %.cc $(REP_DIR)/src/app/wireguard
vpath %.cc $(REP_DIR)/src/lib

# musl-libc contrib sources
MUSL_TM  = $(call select_from_repositories,src/lib/musl_tm)
SRC_C   += tm_to_secs.c
INC_DIR += $(MUSL_TM)

vpath tm_to_secs.c $(MUSL_TM)
