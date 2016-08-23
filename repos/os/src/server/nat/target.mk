TARGET   = nat
LIBS    += base net config server
SRC_CC  += arp_waiter.cc ip_route.cc proxy_role.cc
SRC_CC  += port_allocator.cc port_route.cc component.cc
SRC_CC  += mac.cc main.cc uplink.cc interface.cc arp_cache.cc
INC_DIR += $(PRG_DIR)

vpath *.cc $(REP_DIR)/src/server/proxy_arp
