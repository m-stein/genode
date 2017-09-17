/*
 * \brief  Ethernet protocol
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ipv4.h>

using namespace Genode;
using namespace Net;

const Mac_address Ethernet_frame::BROADCAST(0xFF);


template <>
void Packet_log<Ethernet_frame>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.eth) {
	case Packet_log_config::COMPREHENSIVE:

		print(output, "\033[32mETH\033[0m");
		print(output, " src ", _pkt.src());
		print(output, " dst ", _pkt.dst());
		print(output, " typ ", _pkt.type());
		break;

	case Packet_log_config::COMPACT:

		print(output, "\033[32mETH\033[0m");
		print(output, " ",   _pkt.src());
		print(output, " > ", _pkt.dst());
		break;

	case Packet_log_config::SHORT:

		print(output, "\033[32mETH\033[0m");
		break;

	default: ;
	}
	/* print encapsulated packet */
	switch (_pkt.type()) {
	case Ethernet_frame::Type::ARP:

		print(output, " ", packet_log(*_pkt.data<Arp_packet const>(), _cfg));
		break;

	case Ethernet_frame::Type::IPV4:

		print(output, " ", packet_log(*_pkt.data<Ipv4_packet const>(), _cfg));
		break;

	default: ;
	}
}
