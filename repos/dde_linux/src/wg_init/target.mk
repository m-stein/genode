TARGET   := wg_init
REQUIRES := x86_64

CUSTOM_TARGET_DEPS := wg_init.phony

include $(REP_DIR)/src/wg_init/target.inc

wg_init.phony:
	$(VERBOSE)rm -rf wg_private_key
	$(VERBOSE)echo $(WG_1_PRIVATE_KEY) > wg_private_key
	$(VERBOSE)chmod 700 wg_private_key
	$(VERBOSE)sudo ip link add wg0 type wireguard
	$(VERBOSE)sudo ip addr add $(WG_1_TUNNEL_IP)/24 dev wg0
	$(VERBOSE)sudo wg set wg0 private-key wg_private_key listen-port $(WG_1_UDP_PORT)
	$(VERBOSE)sudo ip link set wg0 up
	$(VERBOSE)sudo wg set wg0 peer $(WG_1_PEER_PUBLIC_KEY) allowed-ips $(WG_1_PEER_ALLOWED_IPS) endpoint $(WG_1_PEER_ENDPOINT)
