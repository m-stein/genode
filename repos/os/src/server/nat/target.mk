TARGET    = nat
LIBS      = base net config server
SRC_CC    = attribute.cc port_allocator.cc vlan.cc component.cc address_node.cc mac.cc main.cc uplink.cc packet_handler.cc
INC_DIR  += $(PRG_DIR)

vpath *.cc $(REP_DIR)/src/server/proxy_arp
