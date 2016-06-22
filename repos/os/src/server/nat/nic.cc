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


Tcp_link_node * Net::Nic::_new_tcp_link(Mac_address cm, Ipv4_address ci, uint16_t cp,
                                        Mac_address sm, Ipv4_address si, uint16_t sp)
{
	Allocator * alloc = env()->heap();
	Tcp_link_node * n = new (alloc) Tcp_link_node(cm, ci, cp, sm, si, sp);
	vlan().tcp_link_list()->insert(n);
	return n;
}


void Net::Nic::_handle_tcp(Ethernet_frame * eth, size_t eth_size,
                           Ipv4_packet * ip, size_t ip_size)
{
	/* get ports */
	size_t tcp_size = ip_size - sizeof(Ipv4_packet);
	Tcp_packet * tcp = new (ip->data<void>()) Tcp_packet(tcp_size);
	uint16_t dst_port = tcp->dst_port();
	uint16_t src_port = tcp->src_port();

	/* for the found port, try to find a port to a client of the NAT */
	Port_node * port = vlan().port_tree()->first();
	if (port) { port = port->find_by_address(dst_port); }
	if (!port) { return; }
	Session_component * client = port->component();

	/* try to find an existing link info or create a new one */
	Tcp_link_node * link = vlan().tcp_link_list()->first();
	if (link) { link = link->find(ip->src(), src_port, client->ipv4_address(), dst_port); }
	if (!link) { link = _new_tcp_link(eth->src(), ip->src(), tcp->src_port(), client->mac_address(), client->ipv4_address(), tcp->dst_port()); }

	/* set the NATs MAC as source and the clients MAC and IP as destination */
	eth->src(_mac);
	eth->dst(client->mac_address().addr);
	ip->dst(client->ipv4_address().addr);

	/* re-calculate affected checksums */
	tcp->update_checksum(ip->src(), ip->dst(), tcp_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	client->send(eth, eth_size);
}


void Net::Nic::_handle_udp(Ethernet_frame * eth, size_t eth_size,
                           Ipv4_packet * ip, size_t ip_size)
{
	/* get destination port */
	size_t udp_size = ip_size - sizeof(Ipv4_packet);
	Udp_packet * udp = new (ip->data<void>()) Udp_packet(udp_size);
	uint16_t dst_port = udp->dst_port();

	/* for the found port, try to find a port to a client of the NAT */
	Port_node * node = vlan().port_tree()->first();
	if (node) { node = node->find_by_address(dst_port); }
	if (!node) { return; }

	/* set the NATs MAC as source and the clients MAC and IP as destination */
	eth->src(_mac);
	eth->dst(node->component()->mac_address().addr);
	ip->dst(node->component()->ipv4_address().addr);

	/* re-calculate affected checksums */
	udp->calc_checksum(ip->src(), ip->dst());
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	node->component()->send(eth, eth_size);
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
	_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
	_nic.rx_channel()->sigh_packet_avail(_sink_submit);
	_nic.tx_channel()->sigh_ack_avail(_source_ack);
	_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
	_nic.link_state_sigh(_client_link_state);
}
