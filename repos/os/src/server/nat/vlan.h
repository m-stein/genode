/*
 * \brief  Virtual local network.
 * \author Stefan Kalkowski
 * \date   2010-08-18
 *
 * A database containing all clients sorted by IP and MAC addresses.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VLAN_H_
#define _VLAN_H_

/* Genode includes */
#include <avl_safe.h>
#include <list_safe.h>
#include <util/xml_node.h>

/* local includes */
#include <proxy_role.h>
#include <port_allocator.h>
#include <arp_waiter.h>
#include <arp_cache.h>
#include <interface.h>

namespace Net
{

	/*
	 * The Vlan is a database containing all clients
	 * sorted by IP and MAC addresses.
	 */
	class Vlan
	{
		public:


		private:

			using Xml_node = Genode::Xml_node;

			Interface_tree    _interface_tree;
			Arp_cache          _arp_cache;
			Arp_waiter_list   _arp_waiters;
			Tcp_proxy_role_list   _tcp_proxy_roles;
			Udp_proxy_role_list   _udp_proxy_roles;
			unsigned const    _rtt_sec;

			void _read_ports(Genode::Xml_node & route, char const * name,
			                 Port_allocator & _port_alloc);

		public:

			Vlan(Port_allocator & tcp_port_alloc, Port_allocator & udp_port_alloc);

			Arp_cache          * arp_cache()    { return &_arp_cache;    }
			Arp_waiter_list   * arp_waiters() { return &_arp_waiters; }
			Interface_tree    * interface_tree()  { return &_interface_tree;  }
			Tcp_proxy_role_list   * tcp_proxy_roles() { return &_tcp_proxy_roles; }
			Udp_proxy_role_list   * udp_proxy_roles() { return &_udp_proxy_roles; }

			unsigned rtt_sec() const { return _rtt_sec; }
	};
}

#endif /* _VLAN_H_ */
