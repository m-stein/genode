proc wireguard_start {} {
	puts "Apply host setup for Wireguard"
	set fd [open udhcpd.conf w]
	puts $fd "interface tap0"
	puts $fd "start 10.0.2.3"
	puts $fd "end   10.0.2.4"
	puts $fd "max_leases 2"
	puts $fd "lease_file udhcpd.leases"
	puts $fd "pidfile udhcpd.pid"
	puts $fd "option dns 10.0.2.1 10.0.2.2"
	puts $fd "option subnet 255.255.255.0"
	puts $fd "option router 10.0.2.1"
	close $fd

	set fd [open udhcpd.leases w]
	close $fd

	exec sudo ip tuntap add dev tap0 mode tap user $::tcl_platform(user)
	exec sudo ip address flush dev tap0
	exec sudo ip address add 10.0.2.1/24 dev tap0
	exec sudo ip link set tap0 up

	exec sudo udhcpd udhcpd.conf

	exec echo "0CtU34qsl97IGiYKSO4tMaF/SJvy04zzeQkhZEbZSk0=" > wg_private_key
	exec chmod 700 wg_private_key
	exec sudo ip link add wg0 type wireguard
	exec sudo ip addr add 10.0.9.1/24 dev wg0
	exec sudo wg set wg0 private-key wg_private_key listen-port 55551
	exec sudo ip link set wg0 up
	exec sudo wg set wg0 peer "GrvyALPZ3PQ2AWM+ovxJqnxSqKpmTyqUui5jH+C8I0E=" allowed-ips 10.0.9.2/32 endpoint 10.0.2.3:55552
}

proc wireguard_exit {} {

	puts "Undo host setup for Wireguard"
	exec sudo pkill -F udhcpd.pid
	exec sudo ip link set wg0 down
	exec sudo ip link delete dev wg0
	exec sudo ip link set tap0 down
	exec sudo ip address flush dev tap0
	exec sudo ip link delete tap0
	exec rm -rf wg_private_key udhcpd.conf udhcpd.leases udhcpd.pid
}
