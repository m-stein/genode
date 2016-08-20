/*
 * \brief  Aspect of waiting for an ARP reply
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
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <util/list.h>
#include <nic_session/nic_session.h>

/* local includes */
#include <list_safe.h>

#ifndef _ARP_WAITER_H_
#define _ARP_WAITER_H_

namespace Net
{
	using Packet_descriptor = ::Nic::Packet_descriptor;
	class Interface;
	class Arp_waiter;
	class Arp_cache_entry;
	using Arp_waiter_list = List_safe<Arp_waiter>;
}

class Net::Arp_waiter : public Genode::List<Arp_waiter>::Element
{
	private:

		Interface * const _handler;
		Ipv4_address           _ip_addr;
		Ethernet_frame * const _eth;
		Genode::size_t const   _eth_size;
		Packet_descriptor *    _packet;

	public:

		Arp_waiter(Interface * const handler,
		           Ipv4_address ip_addr,
		           Ethernet_frame * const eth,
		           Genode::size_t const eth_size,
		           Packet_descriptor * p);

		bool new_arp_cache_entry(Arp_cache_entry * entry);

		Interface * handler() const { return _handler; }
		Ethernet_frame * eth() const { return _eth; }
		Genode::size_t eth_size() const { return _eth_size; }
};

#endif /* _ARP_WAITER_H_ */
