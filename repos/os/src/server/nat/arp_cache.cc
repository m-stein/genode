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

/* local includes */
#include <arp_cache.h>

using namespace Net;
using namespace Genode;


Arp_cache_entry::Arp_cache_entry(Ipv4_address ip, Mac_address mac)
:
	_ip(ip), _mac(mac)
{ }


bool Arp_cache_entry::higher(Arp_cache_entry * entry)
{
	return (memcmp(&entry->_ip.addr, &_ip.addr, sizeof(_ip.addr)) > 0);
}


Arp_cache_entry * Arp_cache_entry::find_by_ip(Ipv4_address ip)
{
	if (ip == _ip) { return this; }
	bool side = memcmp(&ip.addr, _ip.addr, sizeof(_ip.addr)) > 0;
	Arp_cache_entry * entry = Avl_node<Arp_cache_entry>::child(side);
	return entry ? entry->find_by_ip(ip) : 0;
}


Arp_cache_entry * Arp_cache::find_by_ip(Ipv4_address ip)
{
	Arp_cache_entry * entry = first();
	if (!entry) { return entry; }
	return entry->find_by_ip(ip);
}
