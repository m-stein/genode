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
#include <packet_handler.h>
#include <port_allocator.h>

namespace Net { class Uplink; }

class Net::Uplink
:
	public Genode::Session_label, public Nic::Packet_allocator,
	public Nic::Connection, public Net::Packet_handler
{
	private:

		enum {
			PKT_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE = Nic::Session::QUEUE_SIZE * PKT_SIZE,
		};

		Ipv4_address _nat_ip_attr();

	public:

		Uplink(Server::Entrypoint&, Net::Vlan &vlan, Port_allocator & port_alloc);

		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink<Nic::Session::Policy> *   sink()   { return rx(); }
		Packet_stream_source<Nic::Session::Policy> * source() { return tx(); }

		void finalize_packet(Ethernet_frame * eth, Genode::size_t eth_size) { }
};

#endif /* _UPLINK_H_ */
