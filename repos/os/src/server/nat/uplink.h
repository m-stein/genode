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

namespace Net
{
	class Arp_packet;
	class Uplink_base;
	class Uplink;
}

class Net::Uplink_base
{
	protected:

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		Nic::Packet_allocator _tx_block_alloc;
		Nic::Connection       _nic;
		Mac_address           _mac;
		Ipv4_address          _nat_ip;
		Mac_address           _nat_mac;

		Uplink_base();
};

class Net::Uplink : public Uplink_base, public Net::Packet_handler
{
	public:

		Uplink(Server::Entrypoint&, Vlan&, Mac_address nat_mac);

		Nic::Connection * nic()        { return &_nic; }
		Mac_address       mac() const  { return _mac; }
		bool              link_state() { return _nic.link_state(); }

		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink<Nic::Session::Policy> *   sink()   { return _nic.rx(); }
		Packet_stream_source<Nic::Session::Policy> * source() { return _nic.tx(); }

		void finalize_packet(Ethernet_frame * eth, Genode::size_t eth_size) { }
};

#endif /* _UPLINK_H_ */
