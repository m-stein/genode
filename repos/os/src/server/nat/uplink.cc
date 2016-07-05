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

using namespace Net;
using namespace Genode;


Net::Uplink::Uplink(Server::Entrypoint &ep, Net::Vlan &vlan, Mac_address nat_mac)
:
	Packet_handler(ep, vlan, "uplink", nat_mac, Uplink_base::_nat_ip, env()->heap())
{
	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
	_nic.link_state_sigh(_client_link_state);
}


Uplink_base::Uplink_base()
:
	_tx_block_alloc(env()->heap()),
	_nic(&_tx_block_alloc, BUF_SIZE, BUF_SIZE),
 	_mac(_nic.mac_address().addr)
{
	enum { MAX_IP_ADDR_LENGTH  = 16, };
	char nat_ip_addr[MAX_IP_ADDR_LENGTH];
	memset(nat_ip_addr, 0, MAX_IP_ADDR_LENGTH);
	Session_label label("label=\"uplink\"");
	Session_policy policy(label);
	policy.attribute("nat_ip_addr").value(nat_ip_addr, sizeof(nat_ip_addr));

	if (nat_ip_addr != 0 && strlen(nat_ip_addr)) {
		_nat_ip = Ipv4_packet::ip_from_string(nat_ip_addr);
		if (_nat_ip == Ipv4_address()) {
			PWRN("Empty or error nat ip address. Skipped.");
		}
	}
}
