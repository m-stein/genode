/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <nic/xml_node.h>
#include <os/server.h>

/* local includes */
#include <component.h>
#include <arp_cache.h>
#include <uplink.h>

using namespace Net;
using namespace Genode;

static bool const verbose = 0;


class Main
{
	private:

		Port_allocator      _tcp_port_alloc;
		Port_allocator      _udp_port_alloc;
		Server::Entrypoint &_ep;
		Interface_tree      _interface_tree;
		Arp_cache           _arp_cache;
		Arp_waiter_list     _arp_waiters;
		Tcp_proxy_list      _tcp_proxys;
		Udp_proxy_list      _udp_proxys;
		unsigned            _rtt_sec;
		Uplink              _uplink;
		Net::Root           _root;

		void _read_ports(Genode::Xml_node &route, char const *name,
		                 Port_allocator &_port_alloc);

	public:

		Main(Server::Entrypoint &ep);
};


void Main::_read_ports(Xml_node &route, char const *name,
                       Port_allocator &port_alloc)
{
	try {
		for (Xml_node port = route.sub_node(name); ; port = port.next(name)) {
			uint16_t const nr = port.attribute_value("dst", 0UL);
			if (!nr) {
				warning("missing 'dst' attribute in port route");
				continue;
			}
			try { port_alloc.alloc_index(nr); }
			catch (Port_allocator::Already_allocated) { continue; }
			if (verbose) {
				log("Reserve ", name, " ", nr); }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }
}


Main::Main(Server::Entrypoint &ep)
:
	_ep(ep), _rtt_sec(config()->xml_node().attribute_value("rtt_sec", 0UL)),

	_uplink(_ep, _tcp_port_alloc, _udp_port_alloc, _tcp_proxys,
	        _udp_proxys, _rtt_sec, _interface_tree, _arp_cache,
	        _arp_waiters),

	_root(_ep, *env()->heap(), _uplink.nat_mac(), _tcp_port_alloc,
	      _udp_port_alloc, _tcp_proxys, _udp_proxys,
	      _rtt_sec, _interface_tree, _arp_cache, _arp_waiters)
{
	if (!_rtt_sec) { warning("missing 'rtt_sec' attribute in config tag"); }

	/* reserve all ports that are used in port routes */
	try {
		Xml_node policy = config()->xml_node().sub_node("policy");
		for (; ; policy = policy.next("policy")) {
			try {
				Xml_node route = policy.sub_node("ip");
				for (; ; route = route.next("ip")) {
					_read_ports(route, "tcp", _tcp_port_alloc);
					_read_ports(route, "udp", _udp_port_alloc);
				}
			} catch (Xml_node::Nonexistent_sub_node) { }
		}
	} catch (Xml_node::Nonexistent_sub_node) { }

	/* announce service */
	env()->parent()->announce(ep.manage(_root));
}


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nat_ep"; }

	size_t stack_size() { return 4096 *sizeof(addr_t); }

	void construct(Entrypoint &ep) { static Main nat(ep); }
}
