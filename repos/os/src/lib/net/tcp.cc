/*
 * \brief  Transmission Control Protocol
 * \author Martin Stein
 * \date   2016-06-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <net/tcp.h>

using namespace Genode;
using namespace Net;


template <>
void Packet_log<Tcp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.tcp) {
	case Packet_log_config::COMPREHENSIVE:

		print(output, "\033[32mTCP\033[0m");
		print(output, " src ",   _pkt.src_port());
		print(output, " dst ",   _pkt.dst_port());
		print(output, " seqn ",  _pkt.seq_nr());
		print(output, " ackn ",  _pkt.ack_nr());
		print(output, " doff ",  _pkt.data_offset());
		print(output, " flg ",   _pkt.flags());
		print(output, " winsz ", _pkt.window_size());
		print(output, " crc ",   _pkt.checksum());
		print(output, " urgp ",  _pkt.urgent_ptr());
		break;

	case Packet_log_config::COMPACT:

		print(output, "\033[32mTCP\033[0m ");
		if (_pkt.fin()) { print(output, "f"); }
		if (_pkt.syn()) { print(output, "s"); }
		if (_pkt.rst()) { print(output, "r"); }
		if (_pkt.psh()) { print(output, "p"); }
		if (_pkt.ack()) { print(output, "a"); }
		if (_pkt.urg()) { print(output, "u"); }
		print(output, " ",   _pkt.src_port());
		print(output, " > ", _pkt.dst_port());
		break;

	case Packet_log_config::SHORT:

		print(output, "\033[32mTCP\033[0m");
		break;

	default: ;
	}
}
