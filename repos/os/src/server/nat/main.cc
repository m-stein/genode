/*
 * \brief  Proxy-ARP for Nic-session
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <cap_session/connection.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic/xml_node.h>
#include <os/config.h>
#include <os/server.h>

/* local includes */
#include <component.h>


struct Main
{
	/*
	 * Server entrypoint
	 * Is a Thread.
	 */
	Server::Entrypoint &ep;

	/*
	 * The Vlan is a database containing all Virtual local network clients
	 * sorted by IP and MAC addresses. Inherits from nothing.
	 */
	Net::Vlan vlan;

	/*
	 * Proxy-ARP NIC session handler that holds a NIC session to the nic_drv
	 * as back end and is a Net::Packet_handler at the front end.
	 * Implementation is local.
	 */
	Net::Nic  nic  = { ep, vlan };

	/*
	 * Root component, handling new NIC session requests. The declaration and
	 * implementation of both Root and Session_component is local.
	 */
	Net::Root root = { ep, nic, Genode::env()->heap() };

	void handle_config()
	{
		/* read MAC address prefix from config file */
		try {
			Nic::Mac_address mac;
			Genode::config()->xml_node().attribute("mac").value(&mac);
			Genode::memcpy(&Net::Mac_allocator::mac_addr_base, &mac,
			               sizeof(Net::Mac_allocator::mac_addr_base));
		} catch(...) {}
	}

	void read_mac()
	{
		Net::Mac_address mac(nic.mac());
		Genode::printf("--- NAT started "
		               "(mac=%02x:%02x:%02x:%02x:%02x:%02x) ---\n",
		               mac.addr[0], mac.addr[1], mac.addr[2],
		               mac.addr[3], mac.addr[4], mac.addr[5]);
	}

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		try {
			handle_config();
			read_mac();
			Genode::env()->parent()->announce(ep.manage(root));
		} catch (Genode::Parent::Service_denied) {
			PERR("Could not connect to uplink NIC");
		}
	}
};


/************
 ** Server **
 ************/

namespace Server {

	char const *name() { return "nat_ep"; }

	size_t stack_size() { return 2048*sizeof(Genode::addr_t); }

	void construct(Entrypoint &ep) { static Main nat(ep); }
}
