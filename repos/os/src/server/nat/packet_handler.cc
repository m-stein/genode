/*
 * \brief  Packet handler handling network packets.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/lock.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/dump.h>

#include <component.h>
#include <packet_handler.h>

using namespace Net;
using namespace Genode;

static const bool verbose = true;


void Packet_handler::_handle_unknown_arp
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_address ip_addr,
	Packet_handler * handler, bool & ack, Packet_descriptor * p)
{
	handler->arp_broadcast(ip_addr);
	vlan().arp_waiters()->insert(new (_allocator) Arp_waiter(this, ip_addr, eth, eth_size, p));
	ack = false;
}


bool Packet_handler::_handle_ip
(
	Ethernet_frame * eth, Genode::size_t eth_size, bool & ack, Packet_descriptor * p)
{
	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet * ip = new (eth->data<void>()) Ipv4_packet(ip_size);
	switch (ip->protocol()) {
	case Tcp_packet::IP_ID: _handle_tcp(eth, eth_size, ip, ip_size, ack, p); break;
	case Udp_packet::IP_ID: _handle_udp(eth, eth_size, ip, ip_size, ack, p); break;
	default: ; }
	return false;
}


void Packet_handler::arp_broadcast(Ipv4_address ip_addr)
{
	using Ethernet_arp = Ethernet_frame_sized<sizeof(Arp_packet)>;
	Ethernet_arp eth_arp(Mac_address(0xff), _nat_mac, Ethernet_frame::ARP);

	void * const eth_data = eth_arp.data<void>();
	size_t const arp_size = sizeof(eth_arp) - sizeof(Ethernet_frame);
	Arp_packet * const arp = new (eth_data) Arp_packet(arp_size);

	arp->hardware_address_type(Arp_packet::ETHERNET);
	arp->protocol_address_type(Arp_packet::IPV4);
	arp->hardware_address_size(sizeof(Mac_address));
	arp->protocol_address_size(sizeof(Ipv4_address));
	arp->opcode(Arp_packet::REQUEST);
	arp->src_mac(_nat_mac);
	arp->src_ip(_nat_ip);
	arp->dst_mac(Mac_address(0xff));
	arp->dst_ip(ip_addr);

	send(&eth_arp, sizeof(eth_arp));
}


void Packet_handler::_remove_arp_waiter(Arp_waiter * arp_waiter)
{
	vlan().arp_waiters()->remove(arp_waiter);
	destroy(arp_waiter->handler()->allocator(), arp_waiter);
}


Arp_waiter * Packet_handler::_new_arp_node(Arp_waiter * arp_waiter, Arp_node * arp_node)
{
	Arp_waiter * next_arp_waiter = arp_waiter->next();
	if (arp_waiter->new_arp_node(arp_node)) { _remove_arp_waiter(arp_waiter); }
	return next_arp_waiter;
}


void Packet_handler::_handle_arp_reply(Arp_packet * const arp)
{
	/* if an appropriate ARP node doesn't exist jet, create one */
	Arp_node * arp_node = vlan().arp_tree()->first();
	if (arp_node) { arp_node = arp_node->find_by_ip(arp->src_ip()); }
	if (arp_node) { return; }
	arp_node = new (env()->heap()) Arp_node(arp->src_ip(), arp->src_mac());
	vlan().arp_tree()->insert(arp_node);

	/* announce the existence of a new ARP node */
	Arp_waiter * arp_waiter = vlan().arp_waiters()->first();
	for (; arp_waiter; arp_waiter = _new_arp_node(arp_waiter, arp_node)) { }
}


void Packet_handler::_handle_arp_request
(
	Ethernet_frame * const eth, size_t const eth_size, Arp_packet * const arp)
{
	/* ignore packets that do not target the NAT */
	if (arp->dst_ip() != nat_ip()) { return; }

	/* interchange source and destination MAC and IP addresses */
	arp->dst_ip(arp->src_ip());
	arp->dst_mac(arp->src_mac());
	eth->dst(eth->src());
	arp->src_ip(nat_ip());
	arp->src_mac(nat_mac());
	eth->src(nat_mac());

	/* mark packet as reply and send it back to its sender */
	arp->opcode(Arp_packet::REPLY);
	send(eth, eth_size);
}


bool Packet_handler::_handle_arp(Ethernet_frame * eth, size_t eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet *arp = new (eth->data<void>()) Arp_packet(arp_size);
	if (!arp->ethernet_ipv4()) { return false; }

	switch (arp->opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(arp);
	case Arp_packet::REQUEST: _handle_arp_request(eth, eth_size, arp);
	default: ; }
	return false;
}


void Packet_handler::_ready_to_submit(unsigned)
{
	/* as long as packets are available, and we can ack them */
	while (sink()->packet_avail()) {
		_packet = sink()->get_packet();
		if (!_packet.size()) continue;
		bool ack = true;

		if (verbose) {
			Genode::printf("<< ");
			dump_eth(sink()->packet_content(_packet), _packet.size());
			Genode::printf("\n");
		}
		handle_ethernet(sink()->packet_content(_packet), _packet.size(), ack, &_packet);

		if (!ack) { continue; }

		if (!sink()->ready_to_ack()) {
			if (verbose)
				PWRN("ack state FULL");
			return;
		}

		sink()->acknowledge_packet(_packet);
	}
}

void Packet_handler::continue_handle_ethernet(void* src, Genode::size_t size, Packet_descriptor * p)
{
	bool ack = true;
	handle_ethernet(src, size, ack, p);
	if (!ack) { PERR("Failed to continue eth handling"); return; }

	if (!sink()->ready_to_ack()) {
		if (verbose)
			PWRN("ack state FULL");
		return;
	}
	sink()->acknowledge_packet(*p);
}


void Packet_handler::_ready_to_ack(unsigned)
{
	/* check for acknowledgements */
	while (source()->ack_avail())
		source()->release_packet(source()->get_acked_packet());
}


void Packet_handler::_link_state(unsigned)
{
	Mac_address_node *node = _vlan.mac_list()->first();
	while (node) {
		node->component()->link_state_changed();
		node = node->next();
	}
}


void Packet_handler::broadcast_to_clients(Ethernet_frame *eth, Genode::size_t size)
{
	/* check whether it's really a broadcast packet */
	if (eth->dst() == Ethernet_frame::BROADCAST) {
		/* iterate through the list of clients */
		Mac_address_node *node =
			_vlan.mac_list()->first();
		while (node) {
			/* deliver packet */
			node->component()->send(eth, size);
			node = node->next();
		}
	}
}


void Packet_handler::_handle_udp_to_nat
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip, size_t ip_size,
	bool & ack, Packet_descriptor * p)
{
	/* get destination port */
	size_t udp_size = ip_size - sizeof(Ipv4_packet);
	Udp_packet * udp = new (ip->data<void>()) Udp_packet(udp_size);
	uint16_t dst_port = udp->dst_port();

	/* for the found port, try to find a route to a client of the NAT */
	Port_node * node = vlan().port_tree()->first();
	if (node) { node = node->find_by_address(dst_port); }
	if (!node) { return; }
	Session_component * client = node->component();

	/* set the NATs MAC as source and the clients MAC and IP as destination */
	eth->src(_nat_mac);
	eth->dst(client->mac_address().addr);
	ip->dst(client->ipv4_address().addr);

	/* re-calculate affected checksums */
	udp->update_checksum(ip->src(), ip->dst());
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	client->send(eth, eth_size);
}


Packet_handler * Packet_handler::_ip_routing
(
	Ipv4_address & ip_addr, Ipv4_packet * ip)
{
	/* try to find IP routing rule */
	Route_node * ip_route = vlan().ip_routes()->longest_prefix_match(ip->dst());
	if (!ip_route) { return nullptr; }

	/* if a via ip is specified, use it, otherwise use the packet destination */
	if (ip_route->gateway() == Ipv4_address()) { ip_addr = ip->dst(); }
	else { ip_addr = ip_route->gateway(); }

	/* try to find the packet handler behind the given interface name */
	Interface_node * interface = static_cast<Interface_node *>(vlan().interfaces()->first());
	if (!interface) { return nullptr; }
	interface = static_cast<Interface_node *>(interface->find_by_name(ip_route->interface().string()));
	if (!interface) { return nullptr; }
	return interface->handler();
}


void Packet_handler::_handle_udp
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip, size_t ip_size,
	bool & ack, Packet_descriptor * p)
{
	if (ip->dst() == _nat_ip) { _handle_udp_to_nat(eth, eth_size, ip, ip_size, ack, p); }
	else { _handle_to_others(eth, eth_size, ip, ip_size, ack, p); }
}


void Packet_handler::_handle_tcp
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip, size_t ip_size,
	bool & ack, Packet_descriptor * p)
{
	if (ip->dst() == nat_ip()) { _handle_tcp_to_nat(eth, eth_size, ip, ip_size, ack, p); }
	else { _handle_to_others(eth, eth_size, ip, ip_size, ack, p); }
}

void Packet_handler::_handle_known_arp
(
	Ethernet_frame * const eth, size_t const eth_size, Ipv4_packet * const ip,
	size_t const ip_size, Arp_node * const arp_node, Packet_handler * handler)
{
	Mac_address nat_mac = handler->nat_mac();
	Ipv4_address nat_ip = handler->nat_ip();

	/* set the NATs MAC as source and the next hops MAC and IP as destination */
	eth->src(nat_mac);
	ip->src(nat_ip);
	eth->dst(arp_node->mac().addr);

	/* re-calculate affected checksums */
	switch (ip->protocol()) {
	case Tcp_packet::IP_ID: ip->data<Tcp_packet>()->update_checksum(nat_ip, ip->dst(), ip_size - sizeof(Ipv4_packet)); break;
	case Udp_packet::IP_ID: ip->data<Udp_packet>()->update_checksum(nat_ip, ip->dst()); break;
	default: ; }
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet */
	handler->send(eth, eth_size);
}

void Packet_handler::_handle_to_others
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip, size_t ip_size,
	bool & ack, Packet_descriptor * p)
{
	Ipv4_address ip_addr;
	Packet_handler * handler = _ip_routing(ip_addr, ip);
	if (!handler) { return; }

	/* for the found IP find an ARP rule or send an ARP request */
	Arp_node * arp_node = vlan().arp_tree()->first();
	if (arp_node) { arp_node = arp_node->find_by_ip(ip_addr); }
	if (arp_node) { _handle_known_arp(eth, eth_size, ip, ip_size, arp_node, handler); }
	else { _handle_unknown_arp(eth, eth_size, ip_addr, handler, ack, p); }
}


void Packet_handler::_handle_tcp_to_nat
(
	Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip, size_t ip_size,
	bool & ack, Packet_descriptor * p)
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
	eth->src(nat_mac());
	eth->dst(client->mac_address().addr);
	ip->dst(client->ipv4_address().addr);

	/* re-calculate affected checksums */
	prot->update_checksum(ip->src(), ip->dst(), prot_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet to the client */
	client->send(eth, eth_size);
}


void Packet_handler::handle_ethernet(void* src, Genode::size_t size, bool & ack, Packet_descriptor * p)
{
	try {
		/* parse ethernet frame header */
		Ethernet_frame *eth = new (src) Ethernet_frame(size);
		switch (eth->type()) {
		case Ethernet_frame::ARP:
			if (!_handle_arp(eth, size)) return;
			break;
		case Ethernet_frame::IPV4:
			if(!_handle_ip(eth, size, ack, p)) return;
			break;
		default:
			;
		}

		broadcast_to_clients(eth, size);
		finalize_packet(eth, size);
	} catch(Arp_packet::No_arp_packet) {
		PWRN("Invalid ARP packet!");
	} catch(Ethernet_frame::No_ethernet_frame) {
		PWRN("Invalid ethernet frame");
	} catch(Dhcp_packet::No_dhcp_packet) {
		PWRN("Invalid IPv4 packet!");
	} catch(Ipv4_packet::No_ip_packet) {
		PWRN("Invalid IPv4 packet!");
	} catch(Udp_packet::No_udp_packet) {
		PWRN("Invalid UDP packet!");
	}
}


void Packet_handler::send(Ethernet_frame *eth, Genode::size_t size)
{
	if (verbose) {
		Genode::printf(">> ");
		dump_eth(eth, size);
		Genode::printf("\n");
	}
	try {
		/* copy and submit packet */
		Packet_descriptor packet  = source()->alloc_packet(size);
		char             *content = source()->packet_content(packet);
		Genode::memcpy((void*)content, (void*)eth, size);
		source()->submit_packet(packet);
	} catch(Packet_stream_source< ::Nic::Session::Policy>::Packet_alloc_failed) {
		if (verbose)
			PWRN("Packet dropped");
	}
}


Packet_handler::Packet_handler(Server::Entrypoint &ep, Vlan &vlan, char const * name, Mac_address nat_mac, Ipv4_address nat_ip, Genode::Allocator * allocator)
: Interface_node(this, name),
  _vlan(vlan),
  _sink_ack(ep, *this, &Packet_handler::_ack_avail),
  _sink_submit(ep, *this, &Packet_handler::_ready_to_submit),
  _source_ack(ep, *this, &Packet_handler::_ready_to_ack),
  _source_submit(ep, *this, &Packet_handler::_packet_avail),
  _client_link_state(ep, *this, &Packet_handler::_link_state),
  _nat_mac(nat_mac), _nat_ip(nat_ip),
  _allocator(allocator)
{
	vlan.interfaces()->insert(this);
}
