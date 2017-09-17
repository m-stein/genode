/*
 * \brief  Address resolution protocol
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2010-08-24
 *
 * ARP is used to determine a network host's link layer or
 * hardware address when only its Network Layer address is known.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/arp.h>

using namespace Genode;
using namespace Net;


template <>
void Packet_log<Arp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.arp) {
	case Packet_log_config::COMPREHENSIVE:

		print(output, "\033[32mARP\033[0m");
		print(output, " hw ",     _pkt.hardware_address_type());
		print(output, " prot ",   _pkt.protocol_address_type());
		print(output, " hwsz ",   _pkt.hardware_address_size());
		print(output, " protsz ", _pkt.protocol_address_size());
		print(output, " op ",     _pkt.opcode());

		if (_pkt.ethernet_ipv4()) {

			print(output, " srcmac ", _pkt.src_mac());
			print(output, " srcip ",  _pkt.src_ip());
			print(output, " dstmac ", _pkt.dst_mac());
			print(output, " dstip ",  _pkt.dst_ip());

		} else {
			print(output, " ...");
		}
		break;

	case Packet_log_config::COMPACT:

		print(output, "\033[32mARP\033[0m ", _pkt.opcode());

		if (_pkt.ethernet_ipv4()) {

			print(output, " ", _pkt.src_mac());
			print(output, " ", _pkt.src_ip());
			print(output, " > ");

			if (_pkt.opcode() != Arp_packet::REQUEST) {
				print(output, _pkt.dst_mac(), " ");
			}
			print(output, _pkt.dst_ip());

		} else {
			print(output, " ...");
		}
		break;

	case Packet_log_config::SHORT:

		print(output, "\033[32mARP\033[0m");
		break;

	default: ;
	}
}
