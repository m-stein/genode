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

#include <base/env.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/dhcp.h>
#include <os/config.h>

#include <component.h>
#include <attribute.h>

using namespace Net;
using namespace Genode;


Ipv4_address Net::Uplink::_nat_ip_attr() {

	Session_policy policy(*static_cast<Session_label *>(this));
	return ip_attr("src", policy);
}


Net::Uplink::Uplink
(
	Server::Entrypoint &ep, Net::Vlan &vlan, Port_allocator & tcp_port_alloc, Port_allocator & udp_port_alloc)
:
	Session_label(label_from_args("label=\"uplink\"")),
	Nic::Packet_allocator(env()->heap()),
	Nic::Connection(this, BUF_SIZE, BUF_SIZE),
	Interface(
		ep, vlan, mac_address().addr, _nat_ip_attr(), env()->heap(),
		*this, tcp_port_alloc, udp_port_alloc, Mac_address())
{
	rx_channel()->sigh_ready_to_ack(_sink_ack);
	rx_channel()->sigh_packet_avail(_sink_submit);
	tx_channel()->sigh_ack_avail(_source_ack);
	tx_channel()->sigh_ready_to_submit(_source_submit);
}
