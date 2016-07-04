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


Net::Nic::Nic(Server::Entrypoint &ep, Net::Vlan &vlan, Mac_address nat_mac)
:
	Packet_handler(ep, vlan, "uplink", nat_mac, Net::Nic_base::_nat_ip, env()->heap())
{
	class Bad_ip_addr_attr : Genode::Exception { };
	class Bad_netmask_attr : Genode::Exception { };
	class Bad_gateway_attr : Genode::Exception { };

	try {
		Xml_node xn_route = config()->xml_node().sub_node("route");
		for (;;) {
			enum { ADDR_STR_SZ = 16 };

			Ipv4_address ip_addr;
			char ip_addr_str[ADDR_STR_SZ] = { 0 };
			xn_route.attribute("ip_addr").value(ip_addr_str, ADDR_STR_SZ);
			if (ip_addr_str == 0 || !Genode::strlen(ip_addr_str)) { throw Bad_ip_addr_attr(); }
			ip_addr = Ipv4_packet::ip_from_string(ip_addr_str);

			Ipv4_address netmask;
			char netmask_str[ADDR_STR_SZ] = { 0 };
			xn_route.attribute("netmask").value(netmask_str, ADDR_STR_SZ);
			if (netmask_str == 0 || !Genode::strlen(netmask_str)) { throw Bad_netmask_attr(); }
			netmask = Ipv4_packet::ip_from_string(netmask_str);

			Ipv4_address gateway;
			char gateway_str[ADDR_STR_SZ] = { 0 };
			xn_route.attribute("gateway").value(gateway_str, ADDR_STR_SZ);
			if (gateway_str == 0 || !Genode::strlen(gateway_str)) { throw Bad_gateway_attr(); }
			gateway = Ipv4_packet::ip_from_string(gateway_str);

			char const * interface = xn_route.attribute("interface").value_base();
			size_t interface_size = xn_route.attribute("interface").value_size();

			Route_node * route = new (Genode::env()->heap())
				Route_node(ip_addr, netmask, gateway, interface, interface_size);
			vlan.ip_routes()->insert(route);
			xn_route = xn_route.next("route");
		}
	}
	catch (Xml_node::Nonexistent_sub_node) { }

	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
	_nic.link_state_sigh(_client_link_state);
}


Nic_base::Nic_base()
:
	_tx_block_alloc(env()->heap()),
	_nic(&_tx_block_alloc, BUF_SIZE, BUF_SIZE),
 	_mac(_nic.mac_address().addr),
	_nat_mac(_nic.mac_address().addr)
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
