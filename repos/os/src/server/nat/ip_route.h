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
#include <util/list.h>
#include <net/ipv4.h>

/* local includes */
#include <interface_label.h>
#include <port_route.h>

#ifndef _IP_ROUTE_H_
#define _IP_ROUTE_H_

namespace Genode {

	class Xml_node;
	class Allocator;
}

namespace Net {

	class Ip_route;
	class Ip_route_list;
}

class Net::Ip_route : public Genode::List<Ip_route>::Element
{
	private:

		Ipv4_address    _ip_addr;
		Genode::uint8_t _prefix;
		Genode::uint8_t _prefix_bytes;
		Genode::uint8_t _prefix_tail;
		Ipv4_address    _via;
		Ipv4_address    _to;
		Interface_label _label;
		Port_route_tree _udp_port_tree;
		Port_route_tree _tcp_port_tree;
		Port_route_list _udp_port_list;
		Port_route_list _tcp_port_list;

		void _read_tcp_port(Genode::Xml_node & port, Genode::Allocator & alloc);

		void _read_udp_port(Genode::Xml_node & port, Genode::Allocator & alloc);

	public:

		Ip_route(Ipv4_address ip_addr, Genode::uint8_t prefix,
		         Ipv4_address via, Ipv4_address to, char const * label,
		         Genode::size_t label_size, Genode::Allocator & alloc,
		         Genode::Xml_node & route);

		void print(Genode::Output & output) const;

		bool matches(Ipv4_address ip_addr);

		Ipv4_address ip_addr() { return _ip_addr; }
		Ipv4_address via() { return _via; }
		Ipv4_address to() { return _to; }
		Genode::uint8_t prefix() { return _prefix; }
		Interface_label & label() { return _label; }
		Port_route_tree * tcp_port_tree() { return &_tcp_port_tree; }
		Port_route_tree * udp_port_tree() { return &_udp_port_tree; }
		Port_route_list * tcp_port_list() { return &_tcp_port_list; }
		Port_route_list * udp_port_list() { return &_udp_port_list; }
};

class Net::Ip_route_list : public Genode::List<Ip_route>
{
	public:

		Ip_route * longest_prefix_match(Ipv4_address ip_addr);

		void insert(Ip_route * route);
};

#endif /* _IP_ROUTE_H_ */
