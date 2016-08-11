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
#include <attribute.h>
#include <proxy_role.h>

using namespace Net;
using namespace Genode;

static const bool verbose = false;

void log(char const * s)
{
	if (!verbose) { return; }
	PLOG("%s", s);
}

uint16_t tlp_dst_port(uint8_t tlp, void * ptr)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return ((Tcp_packet *)ptr)->dst_port();
	case Udp_packet::IP_ID: return ((Udp_packet *)ptr)->dst_port();
	default: log("Unknown transport protocol"); return 0; }
}


void tlp_dst_port(uint8_t tlp, void * ptr, uint16_t port)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: ((Tcp_packet *)ptr)->dst_port(port); return;
	case Udp_packet::IP_ID: ((Udp_packet *)ptr)->dst_port(port); return;
	default: log("Unknown transport protocol"); }
}


uint16_t tlp_src_port(uint8_t tlp, void * ptr)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return ((Tcp_packet *)ptr)->src_port();
	case Udp_packet::IP_ID: return ((Udp_packet *)ptr)->src_port();
	default: log("Unknown transport protocol"); return 0; }
}


void * tlp_packet(uint8_t tlp, Ipv4_packet * ip, size_t size)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return new (ip->data<void>()) Tcp_packet(size);
	case Udp_packet::IP_ID: return new (ip->data<void>()) Udp_packet(size);
	default: log("Unknown transport protocol"); return nullptr; }
}


Port_tree * tlp_port_tree(uint8_t tlp, Route_node * route)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return route->tcp_port_tree();
	case Udp_packet::IP_ID: return route->udp_port_tree();
	default: log("Unknown transport protocol"); return nullptr; }
}


void Packet_handler::tlp_port_proxy
(
	uint8_t tlp, void * ptr, Ipv4_packet * ip, Ipv4_address client_ip,
	uint16_t client_port)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: {

		Tcp_packet * tcp = (Tcp_packet *)ptr;
		Proxy_role * role = _find_proxy_role_by_client(client_ip, client_port);
		if (!role) { role = _new_proxy_role(client_port, client_ip, ip->src()); }
		role->tcp_packet(ip, tcp);
		tcp->src_port(role->proxy_port());
		return;
	}
	case Udp_packet::IP_ID: log("Can not use proxy ports on UDP"); return;
	default: log("Unknown transport protocol"); }
}


void tlp_update_checksum
(
	uint8_t tlp, void * ptr, Ipv4_address src, Ipv4_address dst, size_t size)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: ((Tcp_packet *)ptr)->update_checksum(src, dst, size); return;
	case Udp_packet::IP_ID: ((Udp_packet *)ptr)->update_checksum(src, dst); return;
	default: log("Unknown transport protocol"); }
}


Packet_handler * Packet_handler::_tlp_proxy_route
(
	uint8_t tlp, void * ptr, uint16_t & dst_port, Ipv4_packet * ip,
	Ipv4_address & ip_dst)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: {

		Tcp_packet * tcp = (Tcp_packet *)ptr;
		Proxy_role * role = _find_proxy_role_by_proxy(ip->dst(), dst_port);
		if (!role) { return nullptr; }
		role->tcp_packet(ip, tcp);
		dst_port = role->client_port();
		ip_dst = role->client_ip();
		return &role->client();
	}
	case Udp_packet::IP_ID: return nullptr;
	default: return nullptr; }
}


void Packet_handler::_delete_proxy_role(Proxy_role * const role)
{
	vlan().proxy_roles()->remove(role);
	unsigned const proxy_port = role->proxy_port();
	destroy(_allocator, role);
	_port_alloc.free(proxy_port);
	_proxy_ports_used--;
}


void Packet_handler::_too_many_proxy_roles()
{
	PERR("To many proxy roles requested");
	class Too_many_proxy_roles : public Exception { };
	throw Too_many_proxy_roles();
}


Proxy_role * Packet_handler::_new_proxy_role
(
	unsigned const client_port, Ipv4_address client_ip, Ipv4_address proxy_ip)
{
	if (_proxy_ports_used == _proxy_ports) { _too_many_proxy_roles(); }
	_proxy_ports_used++;
	unsigned const proxy_port = _port_alloc.alloc();
	Proxy_role * const role = new (_allocator) Proxy_role(
		client_port, proxy_port, client_ip, proxy_ip, *this, _ep,
		vlan().rtt_sec());
	vlan().proxy_roles()->insert(role);
	return role;
}


bool Packet_handler::_chk_delete_proxy_role(Proxy_role * & role)
{
	if (!role->del()) { return false; }
	Proxy_role * const next_role = role->next();
	_delete_proxy_role(role);
	role = next_role;
	return true;
}



void Packet_handler::_handle_ip
(
	Ethernet_frame * eth, Genode::size_t eth_size, bool & ack_packet,
	Packet_descriptor * packet)
{
	/* prepare routing information */
	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet * ip;
	try { ip = new (eth->data<void>()) Ipv4_packet(ip_size); }
	catch (Ipv4_packet::No_ip_packet) { log("Invalid IP packet"); return; }
	Route_node * route = _ip_routes.longest_prefix_match(ip->dst());
	uint8_t tlp = ip->protocol();
	size_t  tlp_size = ip_size - sizeof(Ipv4_packet);
	void *  tlp_ptr = tlp_packet(tlp, ip, tlp_size);
	uint16_t dst_port = tlp_dst_port(tlp, tlp_ptr);
	Packet_handler * handler = nullptr;
	Ipv4_address ip_dst = ip->dst();

	/* try to route packet: first by port, then by IP route, then by proxy role */
	if (route) {
		Port_node * port = tlp_port_tree(tlp, route)->find_by_nr(dst_port);
		if (port) {
			handler = _find_by_label(port->label().string());
			if (handler && port->via() != Ipv4_address()) { ip_dst = port->via(); }
		}
		if (!handler) {
			handler = _find_by_label(route->label().string());
			if (handler && route->via() != Ipv4_address()) {  ip_dst = route->via(); }
		}
	}
	if (!handler) {
		handler = _tlp_proxy_route(tlp, tlp_ptr, dst_port, ip, ip_dst);
	}
	if (!handler) {
		log("Unroutable packet");
		return;
	}
	/* adapt destination MAC address to matching ARP entry or send ARP request */
	Arp_node * arp_node = _vlan.arp_tree()->find_by_ip(ip_dst);
	if (arp_node) { eth->dst(arp_node->mac().addr); }
	else {
		handler->arp_broadcast(ip_dst);
		_vlan.arp_waiters()->insert(new (_allocator)
			Arp_waiter(this, ip_dst, eth, eth_size, packet));
		ack_packet = false;
		log("Matching ARP entry requested");
		return;
	}

	tlp_dst_port(tlp, tlp_ptr, dst_port);
	eth->src(handler->nat_mac());
	ip->dst(ip_dst);

	/* if configured, use proxy source IP */
	if (_proxy) {
		Ipv4_address client_ip = ip->src();
		ip->src(handler->nat_ip());

		/* if source port doesn't match any port forwarding, use a proxy port */
		uint16_t src_port = tlp_src_port(tlp, tlp_ptr);
		Route_node * dst_route = handler->routes()->first();
		for (; dst_route; dst_route = dst_route->next()) {
			if (tlp_port_tree(tlp, dst_route)->find_by_nr(src_port)) { break; } }
		if (!dst_route) { tlp_port_proxy(tlp, tlp_ptr, ip, client_ip, src_port); }
	}
	tlp_update_checksum(tlp, tlp_ptr, ip->src(), ip->dst(), tlp_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));

	/* deliver the modified packet */
	handler->send(eth, eth_size);
}


Proxy_role * Packet_handler::_find_proxy_role_by_client(Ipv4_address ip, uint16_t port)
{
	Proxy_role * role = vlan().proxy_roles()->first();
	while (role) {
		if (_chk_delete_proxy_role(role)) { continue; }
		if (role->matches_client(ip, port)) { break; }
		role = role->next();
	}
	return role;
}


Proxy_role * Packet_handler::_find_proxy_role_by_proxy(Ipv4_address ip, uint16_t port)
{
	Proxy_role * role = vlan().proxy_roles()->first();
	while (role) {
		if (_chk_delete_proxy_role(role)) { continue; }
		if (role->matches_proxy(ip, port)) { break; }
		role = role->next();
	}
	return role;
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
	Arp_node * arp_node = _vlan.arp_tree()->find_by_ip(arp->src_ip());
	if (arp_node) {
		log("ARP entry already exists");
		return;
	}
	arp_node = new (env()->heap()) Arp_node(arp->src_ip(), arp->src_mac());
	_vlan.arp_tree()->insert(arp_node);

	/* announce the existence of a new ARP node */
	Arp_waiter * arp_waiter = vlan().arp_waiters()->first();
	for (; arp_waiter; arp_waiter = _new_arp_node(arp_waiter, arp_node)) { }
}


void Packet_handler::_handle_arp_request
(
	Ethernet_frame * const eth, size_t const eth_size, Arp_packet * const arp)
{
	/* ignore packets that do not target the NAT */
	if (arp->dst_ip() != nat_ip()) { log("ARP does not target NAT"); return; }

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


void Packet_handler::_handle_arp(Ethernet_frame * eth, size_t eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet *arp = new (eth->data<void>()) Arp_packet(arp_size);
	if (!arp->ethernet_ipv4()) { log("ARP for unknown protocol"); return; }

	switch (arp->opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(arp); break;
	case Arp_packet::REQUEST: _handle_arp_request(eth, eth_size, arp); break;
	default: log("unknown ARP operation"); }
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

	if (verbose) { dump_eth(src, size); }
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


Packet_handler * Packet_handler::_find_by_label(char const * label)
{
	if (!strcmp(label, "")) { return nullptr; }
	Interface_node * interface = static_cast<Interface_node *>(_vlan.interfaces()->first());
	if (!interface) { return nullptr; }
	interface = static_cast<Interface_node *>(interface->find_by_name(label));
	if (!interface) { return nullptr; }
	return interface->handler();
}


void Packet_handler::handle_ethernet(void * src, size_t size, bool & ack, Packet_descriptor * p)
{
	try {
		Ethernet_frame * const eth = new (src) Ethernet_frame(size);
		switch (eth->type()) {
		case Ethernet_frame::ARP:  _handle_arp(eth, size); break;
		case Ethernet_frame::IPV4: _handle_ip(eth, size, ack, p); break;
		default: ; }
	}
	catch(Arp_packet::No_arp_packet)         { PWRN("Invalid ARP packet!"); }
	catch(Ethernet_frame::No_ethernet_frame) { PWRN("Invalid ethernet frame"); }
	catch(Dhcp_packet::No_dhcp_packet)       { PWRN("Invalid IPv4 packet!"); }
	catch(Ipv4_packet::No_ip_packet)         { PWRN("Invalid IPv4 packet!"); }
	catch(Udp_packet::No_udp_packet)         { PWRN("Invalid UDP packet!"); }
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


void Packet_handler::_read_route(Xml_node & route_xn)
{
	Ipv4_address ip;
	uint8_t prefix;
	ip_prefix_attr("dst", route_xn, ip, prefix);
	Ipv4_address gw;
	try { gw = ip_attr("via", route_xn); } catch (Bad_ip_attr) { }
	Route_node * route;
	try {
		char const * in = route_xn.attribute("label").value_base();
		size_t in_sz    = route_xn.attribute("label").value_size();
		route = new (_allocator)
			Route_node(ip, prefix, gw, in, in_sz, _allocator, route_xn);
	} catch (Xml_attribute::Nonexistent_attribute) {
		route = new (_allocator)
			Route_node(ip, prefix, gw, "", 0, _allocator, route_xn);
	}
	_ip_routes.insert(route);
}


Packet_handler::Packet_handler
(
	Server::Entrypoint & ep, Vlan & vlan, Mac_address nat_mac,
	Ipv4_address nat_ip, Allocator * allocator, Session_label & label,
	Port_allocator & port_alloc, Mac_address mac, Ipv4_address ip)
:
	Interface_node(this, label.string()), _vlan(vlan), _ep(ep),
	_sink_ack(ep, *this, &Packet_handler::_ack_avail),
	_sink_submit(ep, *this, &Packet_handler::_ready_to_submit),
	_source_ack(ep, *this, &Packet_handler::_ready_to_ack),
	_source_submit(ep, *this, &Packet_handler::_packet_avail),
	_client_link_state(ep, *this, &Packet_handler::_link_state),
	_nat_mac(nat_mac), _nat_ip(nat_ip), _mac(mac), _ip(ip), _allocator(allocator),
	_policy(label), _proxy(false), _proxy_ports(0),
	_proxy_ports_used(0), _port_alloc(port_alloc)
{
	try {
		_proxy_ports = uint_attr("proxy", _policy);
		_proxy = true;
	}
	catch (Bad_uint_attr) { }

	if (verbose) {
		PLOG("Interface \"%s\"", label.string());
		PLOG("  mac    %2x:%2x:%2x:%2x:%2x:%2x ip    %u.%u.%u.%u",
			mac.addr[0], mac.addr[1], mac.addr[2], mac.addr[3], mac.addr[4],
			mac.addr[5], ip.addr[0], ip.addr[1], ip.addr[2], ip.addr[3]);
		PLOG("  natmac %2x:%2x:%2x:%2x:%2x:%2x natip %u.%u.%u.%u",
			_nat_mac.addr[0], _nat_mac.addr[1], _nat_mac.addr[2],
			_nat_mac.addr[3], _nat_mac.addr[4], _nat_mac.addr[5],
			_nat_ip.addr[0], _nat_ip.addr[1], _nat_ip.addr[2],
			_nat_ip.addr[3]);
		PLOG("  proxy %u proxy_ports %u", _proxy, _proxy_ports);
	}
	try {
		Xml_node route = _policy.sub_node("route");
		for (; ; route = route.next("route")) { _read_route(route); }
	} catch (Xml_node::Nonexistent_sub_node) { }

	vlan.interfaces()->insert(this);
}
