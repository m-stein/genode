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

	Session_policy policy(*this);
	return ip_attr("src", policy);
}


Net::Uplink::Uplink
(
	Server::Entrypoint &ep, Net::Vlan &vlan, Mac_address nat_mac,
	Port_allocator & port_alloc)
:
	Session_label("label=\"uplink\""),
	Packet_handler(
		ep, vlan, nat_mac, _nat_ip_attr(), env()->heap(), *this,
		port_alloc, Mac_address(), Ipv4_address()),
	_tx_block_alloc(env()->heap()),
	_nic(&_tx_block_alloc, BUF_SIZE, BUF_SIZE),
	_mac(_nic.mac_address().addr)
{
	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
	_nic.link_state_sigh(_client_link_state);
}
