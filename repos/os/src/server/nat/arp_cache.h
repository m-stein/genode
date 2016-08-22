/*
* \brief  Cache for received ARP information
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
#include <net/ipv4.h>
#include <net/ethernet.h>

/* local includes */
#include <avl_safe.h>

#ifndef _ARP_CACHE_H_
#define _ARP_CACHE_H_

namespace Net {

	class Arp_cache;
	class Arp_cache_entry;
}

class Net::Arp_cache_entry : public Genode::Avl_node<Arp_cache_entry>
{
	private:

		Ipv4_address const _ip;
		Mac_address const _mac;

	public:

		Arp_cache_entry(Ipv4_address ip, Mac_address mac);

		Arp_cache_entry * find_by_ip(Ipv4_address ip);

		bool higher(Arp_cache_entry * entry);

		Ipv4_address ip() const { return _ip; }
		Mac_address mac() const { return _mac; }
};

class Net::Arp_cache : public Avl_tree_safe<Arp_cache_entry>
{
	public:

		Arp_cache_entry * find_by_ip(Ipv4_address ip);
};

#endif /* _ARP_CACHE_H_ */
