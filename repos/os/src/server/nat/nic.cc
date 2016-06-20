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


bool Net::Nic::handle_ip(Ethernet_frame * eth, size_t eth_size) {

	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet * ip = new (eth->data<void>()) Ipv4_packet(ip_size);

	/* try to get a TCP/UDP port from the packet */
	unsigned dst_port = 0;
	switch (ip->protocol())
	{ case Tcp_packet::IP_ID: {

			size_t tcp_size = ip_size - sizeof(Ipv4_packet);
			Tcp_packet * tcp = new (ip->data<void>()) Tcp_packet(tcp_size);
			dst_port = tcp->dst_port();
			break;

	} case Udp_packet::IP_ID: {

			size_t udp_size = ip_size - sizeof(Ipv4_packet);
			Udp_packet * udp = new (ip->data<void>()) Udp_packet(udp_size);
			dst_port = udp->dst_port();
			break;

	} default: ; }
	if (!dst_port) { return false; }

	/* for the found port, try to find a route to a client of the NAT */
	Route_node * node = vlan().route_tree()->first();
	if (!node) { return false; }
	node = node->find_by_address(dst_port);
	if (!node) { return false; }

	/* set the NATs MAC as source and the clients MAC and IP as destination */
	eth->src(_mac);
	eth->dst(node->component()->mac_address().addr);
	ip->dst(node->component()->ipv4_address().addr);

	/* re-calculate TCP/UDP checksum and IP header checksum */
	switch (ip->protocol())
	{ case Tcp_packet::IP_ID: {

			size_t tcp_size = ip_size - sizeof(Ipv4_packet);
			Tcp_packet * tcp = new (ip->data<void>()) Tcp_packet(tcp_size);
			tcp->update_checksum(ip->src(), ip->dst(), tcp_size);
			break;

	} case Udp_packet::IP_ID: {

			size_t udp_size = ip_size - sizeof(Ipv4_packet);
			Udp_packet * udp = new (ip->data<void>()) Udp_packet(udp_size);
			udp->calc_checksum(ip->src(), ip->dst());
			break;

	} default: ; }
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	node->component()->send(eth, eth_size);
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
