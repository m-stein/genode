/*
 * \brief  Address-node holds a client-specific session-component.
 * \author Stefan Kalkowski
 * \date   2010-08-25
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ADDRESS_NODE_H_
#define _ADDRESS_NODE_H_

/* Genode */
#include <avl_safe.h>
#include <util/avl_string.h>
#include <util/list.h>
#include <nic_session/nic_session.h>
#include <net/netaddress.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <util/xml_node.h>

enum { MAX_LABEL_SIZE = 64 };

namespace Net
{
	typedef ::Nic::Packet_descriptor Packet_descriptor;
	class Packet_handler;
	class Session_component;
	class Mac_address_node;
	class Ipv4_address_node;
	class Port_node;
	class Interface_node;
	class Route_node;
	class Route_list;
	class Arp_node;
	class Arp_waiter;
	class Tcp_packet;
	typedef Avl_tree_safe<Port_node> Port_tree;
	enum { INTERFACE_LABEL_SIZE = 64 };
}

class Net::Interface_node
:
	public Genode::String<INTERFACE_LABEL_SIZE>,
	public Genode::Avl_string_base
{
	private:

		Packet_handler * _handler;

	public:

		Interface_node(Packet_handler * handler, char const * name)
		: Genode::String<INTERFACE_LABEL_SIZE>(name), Avl_string_base(string()), _handler(handler) { }

		Packet_handler * handler() { return _handler; }
};

class Net::Port_node : public Genode::Avl_node<Port_node>
{
	private:

		using uint16_t = Genode::uint16_t;
		using Label = Genode::String<MAX_LABEL_SIZE>;

		uint16_t _nr;
		Label    _label;

	public:

		Port_node(uint16_t nr, char const * label, Genode::size_t label_size)
		: _nr(nr), _label(label, label_size) { }

		uint16_t nr() { return _nr; }

		Label & label() { return _label; }

		bool higher(Port_node * p) { return p->_nr > _nr; }

		Port_node * find_by_nr(uint16_t nr)
		{
			using namespace Genode;
			if (nr == _nr) { return this; }
			bool side = nr > _nr;
			Port_node *c = Avl_node<Port_node>::child(side);
			return c ? c->find_by_nr(nr) : 0;
		}
};

class Net::Ipv4_address_node : public Genode::Avl_node<Ipv4_address_node>
{
	private:

		Ipv4_address        _addr;
		Session_component * _component;

	public:

		Ipv4_address_node(Ipv4_address a, Session_component * c)
		: _addr(a), _component(c) { }

		Ipv4_address addr() { return _addr; }

		Session_component * component() { return _component; }

		bool higher(Ipv4_address_node * n)
		{
			using namespace Genode;
			return (memcmp(&n->_addr.addr, &_addr.addr, sizeof(_addr.addr)) > 0);
		}

		Ipv4_address_node * find_by_address(Ipv4_address addr)
		{
			using namespace Genode;
			if (addr == _addr) { return this; }
			bool side = memcmp(&addr.addr, _addr.addr, sizeof(_addr.addr)) > 0;
			Ipv4_address_node *c = Avl_node<Ipv4_address_node>::child(side);
			return c ? c->find_by_address(addr) : 0;
		}
};

class Net::Route_node : public Genode::List<Route_node>::Element
{
	private:

		using size_t = Genode::size_t;
		using Label = Genode::String<MAX_LABEL_SIZE>;

		Ipv4_address    _ip_addr;
		Genode::uint8_t _prefix;
		Genode::uint8_t _prefix_bytes;
		Genode::uint8_t _prefix_tail;
		Ipv4_address    _gateway;
		Label           _label;
		Port_tree       _port_tree;

		void _read_port(Genode::Xml_node & port, Genode::Allocator * alloc);

	public:

		Route_node(Ipv4_address ip_addr, Genode::uint8_t prefix,
		           Ipv4_address gateway, char const * label,
		           size_t label_size, Genode::Allocator * alloc,
		Genode::Xml_node & route);

		void dump();

		bool matches(Ipv4_address ip_addr);

		Ipv4_address ip_addr() { return _ip_addr; }
		Ipv4_address gateway() { return _gateway; }
		size_t prefix() { return _prefix; }
		Label & label() { return _label; }
		Port_tree * port_tree() { return &_port_tree; }
};

class Net::Route_list : public Genode::List<Route_node>
{
	public:

		Route_node * longest_prefix_match(Ipv4_address ip_addr);
		void insert(Route_node * route);
};

class Net::Arp_node : public Genode::Avl_node<Arp_node>
{
	private:

		using uint8_t = Genode::uint8_t;

		Ipv4_address _ip;
		Mac_address  _mac;

	public:

		Arp_node(Ipv4_address a, Mac_address m)
		: _ip(a), _mac(m) { }

		Ipv4_address ip() { return _ip; }
		Mac_address mac() { return _mac; }

		bool higher(Arp_node * n)
		{
			using namespace Genode;
			return (memcmp(&n->_ip.addr, &_ip.addr, sizeof(_ip.addr)) > 0);
		}

		Arp_node * find_by_ip(Ipv4_address ip)
		{
			using namespace Genode;
			if (ip == _ip) { return this; }
			bool side = memcmp(&ip.addr, _ip.addr, sizeof(_ip.addr)) > 0;
			Arp_node *c = Avl_node<Arp_node>::child(side);
			return c ? c->find_by_ip(ip) : 0;
		}
};

class Net::Mac_address_node : public Genode::Avl_node<Mac_address_node>,
                              public Genode::List<Mac_address_node>::Element
{
	private:

		Mac_address         _addr;
		Session_component * _component;

	public:

		Mac_address_node(Mac_address addr, Session_component * component)
		: _addr(addr), _component(component) { }

		Mac_address addr() { return _addr; }

		Session_component * component() { return _component; }

		bool higher(Mac_address_node * c)
		{
			using namespace Genode;
			return (memcmp(&c->_addr.addr, &_addr.addr, sizeof(_addr.addr)) > 0);
		}

		Mac_address_node * find_by_address(Mac_address addr)
		{
			using namespace Genode;
			if (addr == _addr) { return this; }
			bool side = memcmp(&addr.addr, _addr.addr, sizeof(_addr.addr)) > 0;
			Mac_address_node * c = Avl_node<Mac_address_node>::child(side);
			return c ? c->find_by_address(addr) : 0;
		}
};

class Net::Arp_waiter : public Genode::List<Arp_waiter>::Element
{
	private:

		Packet_handler * const _handler;
		Ipv4_address _ip_addr;
		Ethernet_frame * const _eth;
		Genode::size_t const _eth_size;
		Packet_descriptor * _packet;

	public:

		Arp_waiter(Packet_handler * const handler,
		           Ipv4_address ip_addr,
		           Ethernet_frame * const eth,
		           Genode::size_t const eth_size, Packet_descriptor * p);

		Packet_handler * handler() const { return _handler; }
		Ethernet_frame * eth() const { return _eth; }
		Genode::size_t eth_size() const { return _eth_size; }
		bool new_arp_node(Arp_node * arp_node);
};

#endif /* _ADDRESS_NODE_H_ */
