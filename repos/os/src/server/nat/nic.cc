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
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/dhcp.h>
#include <os/config.h>

#include <component.h>

using namespace Net;
using namespace Genode;
using Ipv4_address = Net::Ipv4_packet::Ipv4_address;


bool Net::Nic::handle_arp(Ethernet_frame * eth, size_t eth_size) {

	/* ignore broken packets */
	size_t arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet *arp = new (eth->data<void>()) Arp_packet(arp_size);
	if (!arp->ethernet_ipv4()) { return false; }

	/* ignore operations other than REQUEST */
	if (arp->opcode() != Arp_packet::REQUEST) { return false; }

	/* ignore packets that do not target the NAT IP */
	if (!(arp->dst_ip() == public_ip())) { return false; }

	/* interchange source and destination MAC and IP addresses */
	arp->dst_ip(arp->src_ip());
	arp->dst_mac(arp->src_mac());
	eth->dst(eth->src());
	arp->src_ip(public_ip());
	arp->src_mac(mac());
	eth->src(mac());

	/* mark packet as REPLY */
	arp->opcode(Arp_packet::REPLY);

	/* send packet back to its sender */
	send(eth, eth_size);
	return false;
}


void Net::Nic::_handle_tcp(Ethernet_frame * eth, size_t eth_size,
                           Ipv4_packet * ip, size_t ip_size)
{
	using Protocol = Tcp_packet;

	/* get destination port */
	size_t prot_size = ip_size - sizeof(Ipv4_packet);
	Protocol * prot = new (ip->data<void>()) Protocol(prot_size);
	uint16_t dst_port = prot->dst_port();

	/* for the found port, try to find a route to a client of the NAT */
	Port_node * node = vlan().port_tree()->first();
	if (node) { node = node->find_by_address(dst_port); }
	if (!node) { return; }
	Session_component * client = node->component();

	/* set the NATs MAC as source and the clients MAC and IP as destination */
	eth->src(_mac);
	eth->dst(client->mac_address().addr);
	ip->dst(client->ipv4_address().addr);

	/* re-calculate affected checksums */
	prot->update_checksum(ip->src(), ip->dst(), prot_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	client->send(eth, eth_size);
}


void Net::Nic::_handle_udp(Ethernet_frame * eth, size_t eth_size,
                           Ipv4_packet * ip, size_t ip_size)
{
	using Protocol = Udp_packet;

	/* get destination port */
	size_t prot_size = ip_size - sizeof(Ipv4_packet);
	Protocol * prot = new (ip->data<void>()) Protocol(prot_size);
	uint16_t dst_port = prot->dst_port();

	/* for the found port, try to find a route to a client of the NAT */
	Port_node * node = vlan().port_tree()->first();
	if (node) { node = node->find_by_address(dst_port); }
	if (!node) { return; }
	Session_component * client = node->component();

	/* set the NATs MAC as source and the clients MAC and IP as destination */
	eth->src(_mac);
	eth->dst(client->mac_address().addr);
	ip->dst(client->ipv4_address().addr);

	/* re-calculate affected checksums */
	prot->update_checksum(ip->src(), ip->dst());
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	client->send(eth, eth_size);
}


bool Net::Nic::handle_ip(Ethernet_frame * eth, size_t eth_size)
{
	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet * ip = new (eth->data<void>()) Ipv4_packet(ip_size);
	switch (ip->protocol()) {
	case Tcp_packet::IP_ID: _handle_tcp(eth, eth_size, ip, ip_size); break;
	case Udp_packet::IP_ID: _handle_udp(eth, eth_size, ip, ip_size); break;
	default: ; }
	return false;
}


Net::Nic::Nic(Server::Entrypoint &ep, Net::Vlan &vlan)
:
	Packet_handler(ep, vlan),
	_tx_block_alloc(env()->heap()),
	_nic(&_tx_block_alloc, BUF_SIZE, BUF_SIZE),
	_mac(_nic.mac_address().addr)
{
	_public_ip.addr[0]  =  10;
	_public_ip.addr[1]  =   0;
	_public_ip.addr[2]  =   2;
	_public_ip.addr[3]  =  55;
	_private_ip.addr[0] = 192;
	_private_ip.addr[1] = 168;
	_private_ip.addr[2] =   1;
	_private_ip.addr[3] =   1;


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
			if (ip_addr == Ipv4_packet::Ipv4_address()) { throw Bad_ip_addr_attr(); }

			Ipv4_address netmask;
			char netmask_str[ADDR_STR_SZ] = { 0 };
			xn_route.attribute("netmask").value(netmask_str, ADDR_STR_SZ);
			if (netmask_str == 0 || !Genode::strlen(netmask_str)) { throw Bad_netmask_attr(); }
			netmask = Ipv4_packet::ip_from_string(netmask_str);
			if (netmask == Ipv4_packet::Ipv4_address()) { throw Bad_netmask_attr(); }

			Ipv4_address gateway;
			char gateway_str[ADDR_STR_SZ] = { 0 };
			xn_route.attribute("gateway").value(gateway_str, ADDR_STR_SZ);
			if (gateway_str == 0 || !Genode::strlen(gateway_str)) { throw Bad_gateway_attr(); }
			gateway = Ipv4_packet::ip_from_string(gateway_str);
			if (gateway == Ipv4_packet::Ipv4_address()) { throw Bad_gateway_attr(); }

			Route_node route = * new (Genode::env()->heap()) Route_node(ip_addr, netmask, gateway);
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
