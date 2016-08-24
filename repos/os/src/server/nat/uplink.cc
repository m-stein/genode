/*
 * \brief  NIC handler
 * \author Stefan Kalkowski
 * \date   2013-05-24
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/dhcp.h>
#include <os/config.h>

/* local includes */
#include <component.h>
#include <uplink.h>

using namespace Net;
using namespace Genode;


Ipv4_address Net::Uplink::_nat_ip_attr() {

	Session_policy policy(*static_cast<Session_label *>(this));
	Ipv4_address const ip_addr = policy.attribute_value("src", Ipv4_address());
	if (ip_addr == Ipv4_address()) {
		warning("missing 'src' attribute in policy"); }
	return ip_addr;
}


Net::Uplink::Uplink(Server::Entrypoint  &ep,
                    Port_allocator      &tcp_port_alloc,
                    Port_allocator      &udp_port_alloc,
                    Tcp_proxy_list &tcp_proxys,
                    Udp_proxy_list &udp_proxys,
                    unsigned             rtt_sec,
                    Interface_tree      &interface_tree,
                    Arp_cache           &arp_cache,
                    Arp_waiter_list     &arp_waiters)
:
	Session_label(label_from_args("label=\"uplink\"")),
	Nic::Packet_allocator(env()->heap()),
	Nic::Connection(this, BUF_SIZE, BUF_SIZE),
	Interface(ep, mac_address(), _nat_ip_attr(), *env()->heap(),
	          *static_cast<Session_label *>(this),
	          tcp_port_alloc, udp_port_alloc, Mac_address(),
	          tcp_proxys, udp_proxys, rtt_sec,
	          interface_tree, arp_cache, arp_waiters)
{
	rx_channel()->sigh_ready_to_ack(_sink_ack);
	rx_channel()->sigh_packet_avail(_sink_submit);
	tx_channel()->sigh_ack_avail(_source_ack);
	tx_channel()->sigh_ready_to_submit(_source_submit);
}
