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

/* local includes */
#include <interface_label.h>

namespace Net
{
	class Interface;
	class Session_component;
	class Mac_address_node;
	class Interface;
	class Arp_node;
	class Tcp_packet;
	class Vlan;
}

class Net::Arp_node : public Genode::Avl_node<Arp_node>
{
	private:

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

#endif /* _ADDRESS_NODE_H_ */
