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
#include <read_ip_attr.h>

using namespace Net;
using namespace Genode;


Ipv4_address Net::Uplink::_read_nat_ip_attr() {

	Session_label label("label=\"uplink\"");
	Session_policy policy(label);
	return read_ip_attr("nat_ip_addr", policy);
}


Net::Uplink::Uplink(Server::Entrypoint &ep, Net::Vlan &vlan, Mac_address nat_mac)
:
	Packet_handler(ep, vlan, "uplink", nat_mac, _read_nat_ip_attr(), env()->heap()),
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
