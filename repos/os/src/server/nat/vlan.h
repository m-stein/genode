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

namespace Net {

	class Tcp_peer
	{
		private:

			using Ipv4_address = Net::Ipv4_packet::Ipv4_address;
			using uint16_t     = Genode::uint16_t;

			Mac_address  const _mac;
			Ipv4_address const _ip;
			uint16_t     const _port;

		public:

			Tcp_peer(Mac_address mac, Ipv4_address ip, uint16_t port)
			: _mac(mac), _ip(ip), _port(port) { }

			Mac_address  mac()  { return _mac; }
			Ipv4_address ip()   { return _ip; }
			uint16_t     port() { return _port; }
	};

	class Tcp_link_node : public Genode::List<Tcp_link_node>::Element
	{
		private:

			using Ipv4_address = Net::Ipv4_packet::Ipv4_address;
			using uint16_t     = Genode::uint16_t;

			Tcp_peer _client;
			Tcp_peer _server;

		public:

			Tcp_link_node(Mac_address cm, Ipv4_address ci, uint16_t cp,
			              Mac_address sm, Ipv4_address si, uint16_t sp)
			:
				_client(cm, ci, cp), _server(sm, si, sp)
			{ }

			Tcp_link_node * find(Ipv4_address ci, uint16_t cp,
			                     Ipv4_address si, uint16_t sp)
			{
				using namespace Genode;
				if (cp == _client.port() && sp == _server.port() &&
				    ci == _client.ip()   && si == _server.ip()) { return this; }
				Tcp_link_node * const n = List<Tcp_link_node>::Element::next();
				return n ? n->find(ci, cp, si, sp) : nullptr;
			}

			Tcp_peer & client() { return _client; }
			Tcp_peer & server() { return _server; }
	};

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
			typedef List_safe<Tcp_link_node>         Tcp_link_list;

		private:

			Mac_address_tree  _mac_tree;
			Mac_address_list  _mac_list;
			Ipv4_address_tree _ip_tree;
			Port_tree         _port_tree;
			Tcp_link_list     _tcp_link_list;

		public:

			Vlan() {}

			Mac_address_tree  * mac_tree()      { return &_mac_tree;      }
			Mac_address_list  * mac_list()      { return &_mac_list;      }
			Ipv4_address_tree * ip_tree()       { return &_ip_tree;       }
			Port_tree         * port_tree()     { return &_port_tree;     }
			Tcp_link_list     * tcp_link_list() { return &_tcp_link_list; }
	};
}

#endif /* _VLAN_H_ */
