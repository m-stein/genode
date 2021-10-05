TARGET   := wg_reset
REQUIRES := x86_64

CUSTOM_TARGET_DEPS := wg_reset.phony

wg_reset.phony:
	$(VERBOSE)sudo ip link set wg0 down
	$(VERBOSE)sudo ip link delete dev wg0
