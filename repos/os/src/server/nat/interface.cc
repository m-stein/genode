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

/* Genode includes */
#include <base/lock.h>
#include <net/arp.h>
#include <net/dhcp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <net/dump.h>

/* local includes */
#include <component.h>
#include <interface.h>
#include <attribute.h>
#include <proxy_role.h>
#include <arp_cache.h>

using namespace Net;
using namespace Genode;

static const bool verbose = 0;


uint16_t tlp_dst_port(uint8_t tlp, void * ptr)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return ((Tcp_packet *)ptr)->dst_port();
	case Udp_packet::IP_ID: return ((Udp_packet *)ptr)->dst_port();
	default: if (verbose) { log("Unknown transport protocol"); } }
	return 0;
}


void tlp_dst_port(uint8_t tlp, void * ptr, uint16_t port)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: ((Tcp_packet *)ptr)->dst_port(port); return;
	case Udp_packet::IP_ID: ((Udp_packet *)ptr)->dst_port(port); return;
	default: if (verbose) { log("Unknown transport protocol"); } }
}


bool Interface::_tlp_proxy(uint8_t tlp)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return _tcp_proxy;
	case Udp_packet::IP_ID: return _udp_proxy;
	default: if (verbose) { log("Unknown transport protocol"); } }
	return false;
}


uint16_t tlp_src_port(uint8_t tlp, void * ptr)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return ((Tcp_packet *)ptr)->src_port();
	case Udp_packet::IP_ID: return ((Udp_packet *)ptr)->src_port();
	default: if (verbose) { log("Unknown transport protocol"); } }
	return 0;
}


void * tlp_packet(uint8_t tlp, Ipv4_packet * ip, size_t size)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return new (ip->data<void>()) Tcp_packet(size);
	case Udp_packet::IP_ID: return new (ip->data<void>()) Udp_packet(size);
	default: if (verbose) { log("Unknown transport protocol"); } }
	return nullptr;
}


Port_route_tree * tlp_port_tree(uint8_t tlp, Ip_route * route)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return route->tcp_port_tree();
	case Udp_packet::IP_ID: return route->udp_port_tree();
	default: if (verbose) { log("Unknown transport protocol"); } }
	return nullptr;
}


Port_route_list * tlp_port_list(uint8_t tlp, Ip_route * route)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: return route->tcp_port_list();
	case Udp_packet::IP_ID: return route->udp_port_list();
	default: if (verbose) { log("Unknown transport protocol"); } }
	return nullptr;
}


void Interface::tlp_port_proxy
(
	uint8_t tlp, void * ptr, Ipv4_packet * ip, Ipv4_address client_ip,
	uint16_t client_port)
{
	switch (tlp) {
	case Tcp_packet::IP_ID: {

		Tcp_packet * tcp = (Tcp_packet *)ptr;
		Tcp_proxy_role * role =
			_find_tcp_proxy_role_by_client(client_ip, client_port);
		if (!role) {
			role = _new_tcp_proxy_role(client_port, client_ip, ip->src()); }
		role->tcp_packet(ip, tcp);
		tcp->src_port(role->proxy_port());
		return;
	}
	case Udp_packet::IP_ID: {

		Udp_packet * udp = (Udp_packet *)ptr;
		Udp_proxy_role * role =
			_find_udp_proxy_role_by_client(client_ip, client_port);
		if (!role) {
			role = _new_udp_proxy_role(client_port, client_ip, ip->src()); }
		role->udp_packet(ip, udp);
		udp->src_port(role->proxy_port());
		return;
	}
	default: if (verbose) { log("Unknown transport protocol"); } }
}


void tlp_update_checksum
(
	uint8_t tlp, void * ptr, Ipv4_address src, Ipv4_address dst, size_t size)
{
	switch (tlp) {
	case Tcp_packet::IP_ID:
		((Tcp_packet *)ptr)->update_checksum(src, dst, size);
		return;
	case Udp_packet::IP_ID:
		((Udp_packet *)ptr)->update_checksum(src, dst);
		return;
	default: if (verbose) { log("Unknown transport protocol"); } }
}


Interface * Interface::_tlp_proxy_route
(
	uint8_t tlp, void * ptr, uint16_t & dst_port, Ipv4_packet * ip,
	Ipv4_address & to, Ipv4_address & via)
{
	switch (tlp) {
	case Tcp_packet::IP_ID:
		{
			Tcp_packet * tcp = (Tcp_packet *)ptr;
			Tcp_proxy_role * role =
				_find_tcp_proxy_role_by_proxy(ip->dst(), dst_port);
			if (!role) { return nullptr; }
			role->tcp_packet(ip, tcp);
			dst_port = role->client_port();
			to = role->client_ip();
			via = to;
			if(verbose) { log("Matching TCP proxy role: ", *role); }
			return &role->client();
		}
	case Udp_packet::IP_ID:
		{
			Udp_packet * udp = (Udp_packet *)ptr;
			Udp_proxy_role * role =
				_find_udp_proxy_role_by_proxy(ip->dst(), dst_port);
			if (!role) { return nullptr; }
			role->udp_packet(ip, udp);
			dst_port = role->client_port();
			to = role->client_ip();
			via = to;
			if (verbose) { log("Matching UDP proxy role: ", *role); }
			return &role->client();
		}
	default: if (verbose) { log("Unknown transport protocol"); } }
	return nullptr;
}


void Interface::_delete_tcp_proxy_role(Tcp_proxy_role * const role)
{
	_tcp_proxy_roles.remove(role);
	unsigned const proxy_port = role->proxy_port();
	destroy(_allocator, role);
	_tcp_port_alloc.free(proxy_port);
	_tcp_proxy_ports_used--;
}


void Interface::_delete_udp_proxy_role(Udp_proxy_role * const role)
{
	_udp_proxy_roles.remove(role);
	unsigned const proxy_port = role->proxy_port();
	destroy(_allocator, role);
	_udp_port_alloc.free(proxy_port);
	_udp_proxy_ports_used--;
}


Tcp_proxy_role * Interface::_new_tcp_proxy_role
(
	unsigned const client_port, Ipv4_address client_ip, Ipv4_address proxy_ip)
{
	class Too_many_proxy_roles : public Exception { };
	if (_tcp_proxy_ports_used == _tcp_proxy_ports) {
		throw Too_many_proxy_roles(); }
	_tcp_proxy_ports_used++;
	unsigned const proxy_port = _tcp_port_alloc.alloc();
	Tcp_proxy_role * const role = new (_allocator) Tcp_proxy_role(
		client_port, proxy_port, client_ip, proxy_ip, *this, _ep, _rtt_sec);
	_tcp_proxy_roles.insert(role);
	return role;
}


Udp_proxy_role * Interface::_new_udp_proxy_role
(
	unsigned const client_port, Ipv4_address client_ip, Ipv4_address proxy_ip)
{
	class Too_many_proxy_roles : public Exception { };
	if (_udp_proxy_ports_used == _udp_proxy_ports) {
		throw Too_many_proxy_roles(); }
	_udp_proxy_ports_used++;
	unsigned const proxy_port = _udp_port_alloc.alloc();
	Udp_proxy_role * const role = new (_allocator) Udp_proxy_role(
		client_port, proxy_port, client_ip, proxy_ip, *this, _ep,
		_rtt_sec);
	_udp_proxy_roles.insert(role);
	return role;
}


bool Interface::_chk_delete_tcp_proxy_role(Tcp_proxy_role * & role)
{
	if (!role->del()) { return false; }
	Tcp_proxy_role * const next_role = role->next();
	_delete_tcp_proxy_role(role);
	role = next_role;
	return true;
}

bool Interface::_chk_delete_udp_proxy_role(Udp_proxy_role * & role)
{
	if (!role->del()) { return false; }
	Udp_proxy_role * const next_role = role->next();
	_delete_udp_proxy_role(role);
	role = next_role;
	return true;
}



void Interface::_handle_ip
(
	Ethernet_frame * eth, Genode::size_t eth_size, bool & ack_packet,
	Packet_descriptor * packet)
{
	/* prepare routing information */
	size_t ip_size = eth_size - sizeof(Ethernet_frame);
	Ipv4_packet * ip;
	try { ip = new (eth->data<void>()) Ipv4_packet(ip_size); }
	catch (Ipv4_packet::No_ip_packet) { log("Invalid IP packet"); return; }
	uint8_t tlp = ip->protocol();
	size_t  tlp_size = ip_size - sizeof(Ipv4_packet);
	void *  tlp_ptr = tlp_packet(tlp, ip, tlp_size);
	uint16_t dst_port = tlp_dst_port(tlp, tlp_ptr);
	Interface * handler = nullptr;
	Ipv4_address to = ip->dst();
	Ipv4_address via = ip->dst();

	/* go through all matching IP routes ... */
	Ip_route * route = _ip_routes.first();
	for (; route; route = route->next()) {

		/* ... first try all port routes of the current IP route ... */
		if (!route->matches(ip->dst())) { continue; }
		Port_route * port = tlp_port_list(tlp, route)->first();
		for (; port; port = port->next()) {

			if (port->nr() != dst_port) { continue; }
			handler = _interface_tree.find_by_label(port->label().string());
			if (handler) {

				bool const to_set = port->to() != Ipv4_address();
				bool const via_set = port->via() != Ipv4_address();
				if (to_set && !via_set) {
					to = port->to();
					via = port->to();
					break;
				}
				if (via_set) { via = port->via(); }
				if (to_set) { to = port->to(); }
				break;
			}
		}
		if (handler) { break; }

		/* ... then try the IP route itself ... */
		handler = _interface_tree.find_by_label(route->label().string());
		if (handler) {

			bool const to_set = route->to() != Ipv4_address();
			bool const via_set = route->via() != Ipv4_address();
			if (to_set && !via_set) {
				to = route->to();
				via = route->to();
				break;
			}
			if (via_set) { via = route->via(); }
			if (to_set) { to = route->to(); }
			break;
		}
	}
	/* ... if no port or IP route helps, try to find a matching proxy route ... */
	if (!handler) {
		handler = _tlp_proxy_route(tlp, tlp_ptr, dst_port, ip, to, via);
	}
	/* ... and give up if this also fails */
	if (!handler) {
		if (verbose) { log("Unroutable packet"); }
		return;
	}

	/* send ARP request if there is no ARP entry for the destination IP */
	Arp_cache_entry * arp_entry = _arp_cache.find_by_ip(via);
	if (!arp_entry) {
		handler->arp_broadcast(via);
		_arp_waiters.insert(new (_allocator)
			Arp_waiter(this, via, eth, eth_size, packet));
		ack_packet = false;
		return;
	}
	/* adapt packet to the collected info */
	eth->dst(arp_entry->mac().addr);
	eth->src(handler->nat_mac());
	ip->dst(to);
	tlp_dst_port(tlp, tlp_ptr, dst_port);

	/* if configured, use proxy source IP */
	if (_tlp_proxy(tlp)) {
		Ipv4_address client_ip = ip->src();
		ip->src(handler->nat_ip());

		/* if the source port additionally doesn't match any port forwarding, use a proxy port */
		uint16_t src_port = tlp_src_port(tlp, tlp_ptr);
		Ip_route * dst_route = handler->routes()->first();
		for (; dst_route; dst_route = dst_route->next()) {
			if (tlp_port_tree(tlp, dst_route)->find_by_nr(src_port)) { break; } }
		if (!dst_route) { tlp_port_proxy(tlp, tlp_ptr, ip, client_ip, src_port); }
	}
	/* update checksums and deliver packet */
	tlp_update_checksum(tlp, tlp_ptr, ip->src(), ip->dst(), tlp_size);
	ip->checksum(Ipv4_packet::calculate_checksum(*ip));
	handler->send(eth, eth_size);
}


Tcp_proxy_role * Interface::_find_tcp_proxy_role_by_client(Ipv4_address ip, uint16_t port)
{
	Tcp_proxy_role * role = _tcp_proxy_roles.first();
	while (role) {
		if (_chk_delete_tcp_proxy_role(role)) { continue; }
		if (role->matches_client(ip, port)) { break; }
		role = role->next();
	}
	return role;
}


Tcp_proxy_role * Interface::_find_tcp_proxy_role_by_proxy(Ipv4_address ip, uint16_t port)
{
	Tcp_proxy_role * role = _tcp_proxy_roles.first();
	while (role) {
		if (_chk_delete_tcp_proxy_role(role)) { continue; }
		if (role->matches_proxy(ip, port)) { break; }
		role = role->next();
	}
	return role;
}

Udp_proxy_role * Interface::_find_udp_proxy_role_by_client(Ipv4_address ip, uint16_t port)
{
	Udp_proxy_role * role = _udp_proxy_roles.first();
	while (role) {
		if (_chk_delete_udp_proxy_role(role)) { continue; }
		if (role->matches_client(ip, port)) { break; }
		role = role->next();
	}
	return role;
}


Udp_proxy_role * Interface::_find_udp_proxy_role_by_proxy(Ipv4_address ip, uint16_t port)
{
	Udp_proxy_role * role = _udp_proxy_roles.first();
	while (role) {
		if (_chk_delete_udp_proxy_role(role)) { continue; }
		if (role->matches_proxy(ip, port)) { break; }
		role = role->next();
	}
	return role;
}


void Interface::arp_broadcast(Ipv4_address ip_addr)
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


void Interface::_remove_arp_waiter(Arp_waiter * arp_waiter)
{
	_arp_waiters.remove(arp_waiter);
	destroy(arp_waiter->handler()->allocator(), arp_waiter);
}


Arp_waiter * Interface::_new_arp_entry(Arp_waiter * arp_waiter,
                                             Arp_cache_entry * arp_entry)
{
	Arp_waiter * next_arp_waiter = arp_waiter->next();
	if (arp_waiter->new_arp_cache_entry(arp_entry)) {
		_remove_arp_waiter(arp_waiter);
	}
	return next_arp_waiter;
}


void Interface::_handle_arp_reply(Arp_packet * const arp)
{
	/* if an appropriate ARP node doesn't exist jet, create one */
	Arp_cache_entry * arp_entry = _arp_cache.find_by_ip(arp->src_ip());
	if (arp_entry) {
		if (verbose) { log("ARP entry already exists"); }
		return;
	}
	arp_entry =
		new (env()->heap()) Arp_cache_entry(arp->src_ip(), arp->src_mac());
	_arp_cache.insert(arp_entry);

	/* announce the existence of a new ARP node */
	Arp_waiter * arp_waiter = _arp_waiters.first();
	for (; arp_waiter; arp_waiter = _new_arp_entry(arp_waiter, arp_entry)) { }
}


void Interface::_handle_arp_request
(
	Ethernet_frame * const eth, size_t const eth_size, Arp_packet * const arp)
{
	/* ignore packets that do not target the NAT */
	if (arp->dst_ip() != nat_ip()) {
		if (verbose) { log("ARP does not target NAT"); }
		return;
	}

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


void Interface::_handle_arp(Ethernet_frame * eth, size_t eth_size)
{
	/* ignore ARP regarding protocols other than IPv4 via ethernet */
	size_t arp_size = eth_size - sizeof(Ethernet_frame);
	Arp_packet *arp = new (eth->data<void>()) Arp_packet(arp_size);
	if (!arp->ethernet_ipv4()) {
		if (verbose) { log("ARP for unknown protocol"); return; }
	}

	switch (arp->opcode()) {
	case Arp_packet::REPLY:   _handle_arp_reply(arp); break;
	case Arp_packet::REQUEST: _handle_arp_request(eth, eth_size, arp); break;
	default: if (verbose) { log("unknown ARP operation"); } }
}


void Interface::_ready_to_submit(unsigned)
{
	/* as long as packets are available, and we can ack them */
	while (sink()->packet_avail()) {
		_packet = sink()->get_packet();
		if (!_packet.size()) continue;
		bool ack = true;

		if (verbose) {
			Genode::printf("<< %s ", Interface::string());
			dump_eth(sink()->packet_content(_packet), _packet.size());
			Genode::printf("\n");
		}
		handle_ethernet(sink()->packet_content(_packet), _packet.size(), ack, &_packet);

		if (!ack) { continue; }

		if (!sink()->ready_to_ack()) {
			if (verbose) { log("Ack state FULL"); }
			return;
		}

		sink()->acknowledge_packet(_packet);
	}
}

void Interface::continue_handle_ethernet(void* src, Genode::size_t size, Packet_descriptor * p)
{
	bool ack = true;
	handle_ethernet(src, size, ack, p);
	if (!ack) {
		if (verbose) { log("Failed to continue eth handling"); }
		return;
	}
	if (!sink()->ready_to_ack()) {
		if (verbose) { log("Ack state FULL"); }
		return;
	}
	sink()->acknowledge_packet(*p);
}


void Interface::_ready_to_ack(unsigned)
{
	/* check for acknowledgements */
	while (source()->ack_avail())
		source()->release_packet(source()->get_acked_packet());
}


void Interface::handle_ethernet(void * src, size_t size, bool & ack, Packet_descriptor * p)
{
	try {
		Ethernet_frame * const eth = new (src) Ethernet_frame(size);
		switch (eth->type()) {
		case Ethernet_frame::ARP:  _handle_arp(eth, size); break;
		case Ethernet_frame::IPV4: _handle_ip(eth, size, ack, p); break;
		default: ; }
	}
	catch (Arp_packet::No_arp_packet)         { if (verbose) { log("Invalid ARP packet!"); } }
	catch (Ethernet_frame::No_ethernet_frame) { if (verbose) { log("Invalid ethernet frame"); } }
	catch (Dhcp_packet::No_dhcp_packet)       { if (verbose) { log("Invalid IPv4 packet!"); } }
	catch (Ipv4_packet::No_ip_packet)         { if (verbose) { log("Invalid IPv4 packet!"); } }
	catch (Udp_packet::No_udp_packet)         { if (verbose) { log("Invalid UDP packet!"); } }
}


void Interface::send(Ethernet_frame *eth, Genode::size_t size)
{
	if (verbose) {
		Genode::printf(">> %s ", Interface::string());
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
		if (verbose) { log("Failed to allocate packet"); }
	}
}


void Interface::_read_route(Xml_node & route_xn)
{
	Ipv4_address ip;
	uint8_t prefix;
	ip_prefix_attr("dst", route_xn, ip, prefix);
	Ipv4_address via;
	Ipv4_address to;
	try { via = ip_attr("via", route_xn); } catch (Bad_ip_attr) { }
	try { to = ip_attr("to", route_xn); } catch (Bad_ip_attr) { }
	Ip_route * route;
	try {
		char const * in = route_xn.attribute("label").value_base();
		size_t in_sz    = route_xn.attribute("label").value_size();
		route = new (_allocator)
			Ip_route(ip, prefix, via, to, in, in_sz, _allocator, route_xn);
	} catch (Xml_attribute::Nonexistent_attribute) {
		route = new (_allocator)
			Ip_route(ip, prefix, via, to, "", 0, _allocator, route_xn);
	}
	_ip_routes.insert(route);
	if (verbose) { log("  IP route: ", *route); }
}

Interface::Interface(Server::Entrypoint    &ep,
                     Mac_address            nat_mac,
                     Ipv4_address           nat_ip,
                     Genode::Allocator     &allocator,
                     Genode::Session_label &label,
                     Port_allocator        &tcp_port_alloc,
                     Port_allocator        &udp_port_alloc,
                     Mac_address            mac,
                     Tcp_proxy_role_list   &tcp_proxy_roles,
                     Udp_proxy_role_list   &udp_proxy_roles,
                     unsigned               rtt_sec,
                     Interface_tree        &interface_tree,
                     Arp_cache             &arp_cache,
                     Arp_waiter_list       &arp_waiters)
:
	Interface_label(label.string()),
	Avl_string_base(Interface_label::string()), _ep(ep),
	_sink_ack(ep, *this, &Interface::_ack_avail),
	_sink_submit(ep, *this, &Interface::_ready_to_submit),
	_source_ack(ep, *this, &Interface::_ready_to_ack),
	_source_submit(ep, *this, &Interface::_packet_avail),
	_nat_mac(nat_mac), _nat_ip(nat_ip), _mac(mac), _allocator(allocator),
	_policy(label),
	_tcp_proxy(false),
	_tcp_proxy_ports(0),
	_tcp_proxy_ports_used(0),
	_tcp_proxy_roles(tcp_proxy_roles),
	_tcp_port_alloc(tcp_port_alloc),
	_udp_proxy(false),
	_udp_proxy_ports(0),
	_udp_proxy_ports_used(0),
	_udp_proxy_roles(udp_proxy_roles),
	_udp_port_alloc(udp_port_alloc),
	_rtt_sec(rtt_sec),
	_interface_tree(interface_tree),
	_arp_cache(arp_cache),
	_arp_waiters(arp_waiters)
{
	try {
		_tcp_proxy_ports = uint_attr("tcp-proxy", _policy);
		_tcp_proxy = true;
	}
	catch (Bad_uint_attr) { }
	try {
		_udp_proxy_ports = uint_attr("udp-proxy", _policy);
		_udp_proxy = true;
	}
	catch (Bad_uint_attr) { }
	if (verbose) {
		log("Interface \"", *static_cast<Interface *>(this), "\"");
		log("  MAC ", _mac);
		log("  NAT identity: MAC ", _nat_mac, " IP ", _nat_ip);
		log("  TCP proxy: active ", _tcp_proxy, " ports ", _tcp_proxy_ports);
		log("  UDP proxy: active ", _udp_proxy, " ports ", _udp_proxy_ports);
	}
	try {
		Xml_node route = _policy.sub_node("route");
		for (; ; route = route.next("route")) { _read_route(route); }
	} catch (Xml_node::Nonexistent_sub_node) { }
	_interface_tree.insert(this);
}

Interface::~Interface()
{
	Arp_waiter * arp_waiter = _arp_waiters.first();
	while (arp_waiter) {
		Arp_waiter * next_arp_waiter = arp_waiter->next();
		if (arp_waiter->handler() != this) { _remove_arp_waiter(arp_waiter); }
		arp_waiter = next_arp_waiter;
	}
	_interface_tree.remove(this);
}


Interface * Interface_tree::find_by_label(char const * label)
{
	if (!strcmp(label, "")) { return nullptr; }
	Interface * interface = static_cast<Interface *>(first());
	if (!interface) { return nullptr; }
	interface = static_cast<Interface *>(interface->find_by_name(label));
	return interface;
}
