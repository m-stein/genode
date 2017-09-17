/*
 * \brief  User datagram protocol
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

/* Genode */
#include <net/udp.h>
#include <net/dhcp.h>

using namespace Genode;
using namespace Net;


template <>
void Packet_log<Udp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.udp) {
	case Packet_log_config::COMPREHENSIVE:

		print(output, "\033[32mUDP\033[0m");
		print(output, " src ", _pkt.src_port());
		print(output, " dst ", _pkt.dst_port());
		print(output, " len ", _pkt.length());
		print(output, " crc ", _pkt.checksum());
		break;

	case Packet_log_config::COMPACT:

		print(output, "\033[32mUDP\033[0m");
		print(output, " ",   _pkt.src_port());
		print(output, " > ", _pkt.dst_port());
		break;

	case Packet_log_config::SHORT:

		print(output, "\033[32mUDP\033[0m");
		break;

	default: ;
	}
	/* print encapsulated packet */
	if (Dhcp_packet::is_dhcp(&_pkt)) {
		print(output, " ", packet_log(*_pkt.data<Dhcp_packet const>(), _cfg));
	}
}
