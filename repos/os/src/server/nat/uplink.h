/*
 * \brief  Proxy-ARP NIC session handler
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UPLINK_H_
#define _UPLINK_H_

/* Genode includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include <interface.h>
#include <port_allocator.h>

namespace Net { class Uplink; }

class Net::Uplink
:
	public Genode::Session_label, public Nic::Packet_allocator,
	public Nic::Connection, public Net::Interface
{
	private:

		enum {
			PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE = Nic::Session::QUEUE_SIZE * PKT_SIZE,
		};

		Ipv4_address _nat_ip_attr();

	public:

		Uplink(Server::Entrypoint  &ep,
		       Port_allocator      &tcp_port_alloc,
		       Port_allocator      &udp_port_alloc,
		       Tcp_proxy_role_list &tcp_proxy_roles,
		       Udp_proxy_role_list &udp_proxy_roles,
		       unsigned             rtt_sec,
		       Interface_tree      &interface_tree,
		       Arp_cache           &arp_cache,
		       Arp_waiter_list     &arp_waiters);

		Packet_stream_sink<Nic::Session::Policy> *   sink()   { return rx(); }
		Packet_stream_source<Nic::Session::Policy> * source() { return tx(); }
};

#endif /* _UPLINK_H_ */
