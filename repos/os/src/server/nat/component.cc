/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/udp.h>
#include <net/tcp.h>

#include <component.h>

using namespace Net;
using namespace Genode;

static const int verbose = 1;


bool Session_component::handle_arp(Ethernet_frame *eth, size_t eth_size)
{
	/* ignore broken packets */
	size_t arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet *arp = new (eth->data<void>()) Arp_packet(arp_size);
	if (!arp->ethernet_ipv4()) { return false; }

	/* ignore operations other than REQUEST */
	if (arp->opcode() != Arp_packet::REQUEST) { return false; }

	/* interchange source and destination MAC and IP addresses */
	Ipv4_address old_dst_ip = arp->dst_ip();
	arp->dst_ip(arp->src_ip());
	arp->dst_mac(arp->src_mac());
	eth->dst(eth->src());
	arp->src_ip(old_dst_ip);
	arp->src_mac(_nic.mac());
	eth->src(_nic.mac());

	/* mark packet as REPLY */
	arp->opcode(Arp_packet::REPLY);

	/* send packet back to its sender */
	send(eth, eth_size);
	return false;
}


void Session_component::_arp_broadcast(Ipv4_address ip_addr)
{
	using Ethernet_arp = Ethernet_frame_sized<sizeof(Arp_packet)>;
	Ethernet_arp eth_arp(Mac_address(0xff), _nic.mac(), Ethernet_frame::ARP);

	void * const eth_data = eth_arp.data<void>();
	size_t const arp_size = sizeof(eth_arp) - sizeof(Ethernet_frame);
	Arp_packet * const arp = new (eth_data) Arp_packet(arp_size);

	arp->hardware_address_type(Arp_packet::ETHERNET);
	arp->protocol_address_type(Arp_packet::IPV4);
	arp->hardware_address_size(sizeof(Mac_address));
	arp->protocol_address_size(sizeof(Ipv4_address));
	arp->opcode(Arp_packet::REQUEST);
	arp->src_mac(_nic.mac());
	arp->src_ip(_nic.public_ip());
	arp->dst_mac(Mac_address(0xff));
	arp->dst_ip(ip_addr);

	_nic.send(&eth_arp, sizeof(eth_arp));
}


void Session_component::_handle_tcp_unknown_arp
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_address ip_addr)
{
	_arp_broadcast(ip_addr);
	vlan().arp_waiters()->insert(new (this->guarded_allocator())
		Arp_waiter(this, ip_addr, eth, eth_size));
}


void Session_component::_handle_tcp_known_arp
(
	Ethernet_frame * const eth, size_t const eth_size, Ipv4_packet * const ip,
	size_t const ip_size, Arp_node * const arp_node)
{
	PINF("Found ARP node %x.%x.%x.%x.%x.%x",
		arp_node->mac().addr[0],
		arp_node->mac().addr[1],
		arp_node->mac().addr[2],
		arp_node->mac().addr[3],
		arp_node->mac().addr[4],
		arp_node->mac().addr[5]);

	size_t tcp_size = ip_size - sizeof(Ipv4_packet);
	Tcp_packet * tcp = new (ip->data<void>()) Tcp_packet(tcp_size);

	/* set the NATs MAC as source and the next hops MAC and IP as destination */
	eth->src(_nic.mac());
	ip->src(_nic.public_ip());
	eth->dst(arp_node->mac().addr);

	/* re-calculate affected checksums */
	tcp->update_checksum(_nic.public_ip(), ip->dst(), tcp_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet */
	_nic.send(eth, eth_size);
}


void Session_component::_handle_tcp
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip, size_t ip_size)
{
	/* try to find IP routing rule */
	Route_node * ip_route = vlan().ip_routes()->longest_prefix_match(ip->dst());
	if (!ip_route) { return; }

	/* if a via ip is specified, use it, otherwise use the packet destination */
	Ipv4_address ip_addr;
	if (ip_route->gateway() == Ipv4_address()) {  ip_addr = ip->dst(); }
	else { ip_addr = ip_route->gateway(); }

	/* try to find the given interface */
	Interface_node * interface = static_cast<Interface_node *>(vlan().interfaces()->first());
	if (!interface) { return; }
	interface = static_cast<Interface_node *>(interface->find_by_name(ip_route->interface().string()));
	if (!interface) { return; }

	/* for the found IP find an ARP rule or send an ARP request */
	Arp_node * arp_node = vlan().arp_tree()->first();
	if (arp_node) { arp_node = arp_node->find_by_ip(ip_addr); }
	if (arp_node) { _handle_tcp_known_arp(eth, eth_size, ip, ip_size, arp_node); }
	else { _handle_tcp_unknown_arp(eth, eth_size, ip_addr); }
}

bool Session_component::handle_ip
(
	Ethernet_frame * eth, Genode::size_t eth_size)
{
	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet * ip = new (eth->data<void>()) Ipv4_packet(ip_size);
	switch (ip->protocol()) {
	case Tcp_packet::IP_ID: _handle_tcp(eth, eth_size, ip, ip_size); break;
	case Udp_packet::IP_ID: _handle_udp(eth, eth_size, ip, ip_size); break;
	default: ; }
	return false;
}


void Session_component::_handle_udp(Ethernet_frame * eth, size_t eth_size,
                                    Ipv4_packet * ip, size_t ip_size)
{
}


void Session_component::finalize_packet(Ethernet_frame * eth,
                                        size_t size)
{
	Mac_address_node *node = vlan().mac_tree()->first();
	if (node)
		node = node->find_by_address(eth->dst());
	if (node)
		node->component()->send(eth, size);
	else {
		/* set our MAC as sender */
		eth->src(_nic.mac());
		_nic.send(eth, size);
	}
}


void Session_component::_free_ipv4_node()
{
	if (_ipv4_node) {
		vlan().ip_tree()->remove(_ipv4_node);
		destroy(this->guarded_allocator(), _ipv4_node);
	}
}


bool Session_component::link_state() { return _nic.link_state(); }


void Session_component::set_port(unsigned port)
{
	_free_port_node();
	_port_node = new (this->guarded_allocator())
		Port_node(port, this);
	vlan().port_tree()->insert(_port_node);
}


void Session_component::_free_port_node()
{
	if (_port_node) {
		vlan().port_tree()->remove(_port_node);
		destroy(this->guarded_allocator(), _port_node);
	}
}


void Session_component::set_ipv4_address(Ipv4_address ip_addr)
{
	_free_ipv4_node();
	_ipv4_node = new (this->guarded_allocator())
		Ipv4_address_node(ip_addr, this);
	vlan().ip_tree()->insert(_ipv4_node);
}


Session_component::Session_component(Allocator                  *allocator,
                                     size_t                      amount,
                                     size_t                      tx_buf_size,
                                     size_t                      rx_buf_size,
                                     Ethernet_frame::Mac_address vmac,
                                     Server::Entrypoint         &ep,
                                     Net::Nic                   &nic,
                                     char                       *ip_addr,
                                     unsigned                    port, char const * label)
: Guarded_range_allocator(allocator, amount),
  Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),
  Session_rpc_object(Tx_rx_communication_buffers::tx_ds(),
                     Tx_rx_communication_buffers::rx_ds(),
                     this->range_allocator(), ep.rpc_ep()),
  Packet_handler(ep, nic.vlan(), label),
  _mac_node(vmac, this),
  _ipv4_node(0),
  _port_node(0),
  _nic(nic)
{
	vlan().mac_tree()->insert(&_mac_node);
	vlan().mac_list()->insert(&_mac_node);

	/* static ip parsing */
	if (ip_addr != 0 && strlen(ip_addr)) {
		Ipv4_address ip = Ipv4_packet::ip_from_string(ip_addr);

		if (ip == Ipv4_address() || port == 0) {
			PWRN("Empty or error ip address or port. Skipped.");
		} else {
			set_ipv4_address(ip);
			set_port(port);

			if (verbose)
				PLOG("vmac=%02x:%02x:%02x:%02x:%02x:%02x ip=%d.%d.%d.%d",
				     vmac.addr[0], vmac.addr[1], vmac.addr[2],
				     vmac.addr[3], vmac.addr[4], vmac.addr[5],
				     ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);
		}
	}

	_tx.sigh_ready_to_ack(_sink_ack);
	_tx.sigh_packet_avail(_sink_submit);
	_rx.sigh_ack_avail(_source_ack);
	_rx.sigh_ready_to_submit(_source_submit);
}


Session_component::~Session_component() {
	vlan().mac_tree()->remove(&_mac_node);
	vlan().mac_list()->remove(&_mac_node);
	_free_ipv4_node();
	_free_port_node();
}
