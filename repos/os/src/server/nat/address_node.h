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
#include <util/list.h>
#include <nic_session/nic_session.h>
#include <net/netaddress.h>
#include <net/ethernet.h>
#include <net/ipv4.h>

namespace Net
{
	class Session_component;
	class Mac_address_node;
	class Ipv4_address_node;
	class Port_node;
}

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

		using Ipv4_address = Ipv4_packet::Ipv4_address;

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

#endif /* _ADDRESS_NODE_H_ */
