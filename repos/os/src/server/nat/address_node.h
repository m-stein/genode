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
#include <util/avl_tree.h>
#include <util/avl_string.h>
#include <util/list.h>
#include <nic_session/nic_session.h>
#include <net/netaddress.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

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
}

class Net::Interface_node : public Genode::Avl_string_base
{
	private:

		Packet_handler * _handler;

	public:

		Interface_node(Packet_handler * handler, char const * name)
		: Avl_string_base(name), _handler(handler) { }

		Packet_handler * handler() { return _handler; }
};

class Net::Port_node : public Genode::Avl_node<Port_node>
{
	private:

		using uint16_t = Genode::uint16_t;

		uint16_t            _addr;
		Session_component * _component;

	public:

		Port_node(uint16_t addr, Session_component *component)
		: _addr(addr), _component(component) { }

		uint16_t addr() { return _addr; }

		Session_component *component() { return _component; }

		bool higher(Port_node *c)
		{
			using namespace Genode;
			return c->_addr > _addr;
		}

		Port_node * find_by_address(uint16_t addr)
		{
			using namespace Genode;
			if (addr == _addr) { return this; }
			bool side = addr > _addr;
			Port_node *c = Avl_node<Port_node>::child(side);
			return c ? c->find_by_address(addr) : 0;
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

		enum { MAX_INTERFACE_SIZE = 64 };

		using size_t = Genode::size_t;
		using Interface = Genode::String<MAX_INTERFACE_SIZE>;


		Ipv4_address _ip_addr;
		Ipv4_address _netmask;
		Ipv4_address _gateway;
		size_t       _prefix_size = 0;
		size_t       _prefix_width = 0;
		Interface    _interface;

	public:

		Route_node(Ipv4_address ip_addr, Ipv4_address netmask,
		           Ipv4_address gateway, char const * interface,
		           size_t interface_size);

		void dump();

		bool matches(Ipv4_address ip_addr);

		Ipv4_address ip_addr() { return _ip_addr; }
		Ipv4_address gateway() { return _gateway; }
		size_t prefix_width() { return _prefix_width; }
		Interface & interface() { return _interface; }
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

		Session_component * const _component;
		Ipv4_address _ip_addr;
		Ethernet_frame * const _eth;
		Genode::size_t const _eth_size;
		Packet_descriptor * _packet;

	public:

		Arp_waiter(Session_component * const component,
		           Ipv4_address ip_addr,
		           Ethernet_frame * const eth,
		           Genode::size_t const eth_size, Packet_descriptor * p);

		Session_component * component() const { return _component; }
		Ethernet_frame * eth() const { return _eth; }
		Genode::size_t eth_size() const { return _eth_size; }
		bool new_arp_node(Arp_node * arp_node);
};

#endif /* _ADDRESS_NODE_H_ */
