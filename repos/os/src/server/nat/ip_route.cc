/*
 * \brief  IP routing entry
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/xml_node.h>
#include <base/allocator.h>
#include <base/log.h>

/* local includes */
#include <ip_route.h>
#include <attribute.h>

using namespace Net;
using namespace Genode;

static bool const verbose = 0;


bool Ip_route::matches(Ipv4_address ip_addr)
{
	if (memcmp(&ip_addr.addr, _ip_addr.addr, _prefix_bytes)) { return false; }
	if (!_prefix_tail) { return true; }
	uint8_t ip_tail = ip_addr.addr[_prefix_bytes] & _prefix_tail;
	if (ip_tail != _ip_addr.addr[_prefix_bytes]) { return false; }
	return true;
}


void Ip_route::print(Output & output) const
{
	Genode::print(output, _ip_addr, "/", _prefix, " -> \"",
	              _label, "\" to ", _to, " via ", _via);
}


void Ip_route::_read_tcp_port
(
	Xml_node & port, Allocator & alloc)
{
	uint16_t nr;
	try { nr = uint_attr("nr", port); }
	catch (Bad_uint_attr) { return; }
	Port_route * port_route;
	try {
		char const * label = port.attribute("label").value_base();
		size_t label_size = port.attribute("label").value_size();
		Ipv4_address via;
		Ipv4_address to;
		try { via = ip_attr("via", port); } catch (Bad_ip_attr) { }
		try { to = ip_attr("to", port); } catch (Bad_ip_attr) { }
		port_route = new (&alloc) Port_route(nr, label, label_size, via, to);
	} catch (Xml_attribute::Nonexistent_attribute) {
		Ipv4_address via;
		Ipv4_address to;
		try { via = ip_attr("via", port); } catch (Bad_ip_attr) { }
		try { to = ip_attr("to", port); } catch (Bad_ip_attr) { }
		port_route = new (alloc) Port_route(nr, "", 0, via, to);
	}
	_tcp_port_tree.insert(port_route);
	_tcp_port_list.insert(port_route);
	if (verbose) { log("    TCP port route: ", *port_route); }
}

void Ip_route::_read_udp_port
(
	Xml_node & port, Allocator & alloc)
{
	uint16_t const nr = uint_attr("nr", port);
	char const * label = port.attribute("label").value_base();
	size_t label_size = port.attribute("label").value_size();
	Ipv4_address via;
	Ipv4_address to;
	try { via = ip_attr("via", port); } catch (Bad_ip_attr) { }
	try { to = ip_attr("to", port); } catch (Bad_ip_attr) { }
	Port_route * port_route = new (alloc)
		Port_route(nr, label, label_size, via, to);
	_udp_port_tree.insert(port_route);
	_udp_port_list.insert(port_route);
	if (verbose) { log("    UDP port route: ", *port_route); }
}



Ip_route::Ip_route
(
	Ipv4_address ip_addr, uint8_t prefix, Ipv4_address via, Ipv4_address to,
	char const * label, size_t label_size, Allocator & alloc,
	Xml_node & route)
:
	_ip_addr(ip_addr), _prefix(prefix), _prefix_bytes(_prefix / 8),
	_prefix_tail(~(((uint8_t)~0) >> (_prefix - (_prefix_bytes * 8)))),
	_via(via), _to(to), _label(label, label_size)
{
	if (_prefix_bytes < sizeof(ip_addr.addr)) {
		_ip_addr.addr[_prefix_bytes] &= _prefix_tail; }
	try {
		Xml_node port = route.sub_node("tcp-port");
		for (; ; port = port.next("tcp-port")) { _read_tcp_port(port, alloc); }
	} catch (Xml_node::Nonexistent_sub_node) { }
	try {
		Xml_node port = route.sub_node("udp-port");
		for (; ; port = port.next("udp-port")) { _read_udp_port(port, alloc); }
	} catch (Xml_node::Nonexistent_sub_node) { }
}


Ip_route * Ip_route_list::longest_prefix_match(Ipv4_address ip_addr)
{
	for (Ip_route * curr = first(); curr; curr = curr->next()) {
		if (curr->matches(ip_addr)) { return curr; }
	}
	return nullptr;
}


void Ip_route_list::insert(Ip_route * route)
{
	Ip_route * behind = nullptr;
	for (Ip_route * curr = first(); curr; curr = curr->next()) {
		if (route->prefix() >= curr->prefix()) { break; }
		behind = curr;
	}
	Genode::List<Ip_route>::insert(route, behind);
}
