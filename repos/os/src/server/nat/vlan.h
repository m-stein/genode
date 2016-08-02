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

#include <address_node.h>
#include <avl_safe.h>
#include <list_safe.h>
#include <util/xml_node.h>

#include <proxy_role.h>

namespace Net
{
	/*
	 * The Vlan is a database containing all clients
	 * sorted by IP and MAC addresses.
	 */
	class Vlan
	{
		public:

			typedef Avl_tree_safe<Mac_address_node>  Mac_address_tree;
			typedef Avl_tree_safe<Ipv4_address_node> Ipv4_address_tree;
			typedef List_safe<Mac_address_node>      Mac_address_list;
			typedef Avl_tree_safe<Port_node>         Port_tree;
			typedef Avl_tree_safe<Arp_node>          Arp_tree;
			typedef List_safe<Arp_waiter>            Arp_waiter_list;

			typedef Avl_tree_safe<Genode::Avl_string_base> Interface_tree;
			typedef List_safe<Proxy_role> Proxy_role_list;

		private:

			using Xml_node = Genode::Xml_node;

			Mac_address_tree  _mac_tree;
			Mac_address_list  _mac_list;
			Ipv4_address_tree _ip_tree;
			Port_tree         _port_tree;
			Interface_tree    _interfaces;
			Arp_tree          _arp_tree;
			Arp_waiter_list   _arp_waiters;
			Proxy_role_list   _proxy_roles;
			unsigned const    _rtt_sec;

		public:

			Vlan();

			Mac_address_tree  * mac_tree()    { return &_mac_tree;    }
			Mac_address_list  * mac_list()    { return &_mac_list;    }
			Ipv4_address_tree * ip_tree()     { return &_ip_tree;     }
			Port_tree         * port_tree()   { return &_port_tree;   }
			Arp_tree          * arp_tree()    { return &_arp_tree;    }
			Arp_waiter_list   * arp_waiters() { return &_arp_waiters; }
			Interface_tree    * interfaces()  { return &_interfaces;  }
			Proxy_role_list   * proxy_roles() { return &_proxy_roles; }

			unsigned rtt_sec() const { return _rtt_sec; }
	};
}

#endif /* _VLAN_H_ */
