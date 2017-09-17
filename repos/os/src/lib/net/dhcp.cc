/*
 * \brief  DHCP related definitions
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
#include <net/dhcp.h>

using namespace Genode;
using namespace Net;


template <>
void Packet_log<Dhcp_packet>::print(Output &output) const
{
	using Genode::print;

	/* print header attributes */
	switch (_cfg.dhcp) {
	case Packet_log_config::COMPREHENSIVE:

		print(output, "\033[32mDHCP\033[0m");
		print(output, " op ",   _pkt.op());
		print(output, " htyp ", _pkt.htype());
		print(output, " hlen ", _pkt.hlen());
		print(output, " hps ",  _pkt.hops());
		print(output, " xid ",  _pkt.xid());
		print(output, " sec ",  _pkt.secs());
		print(output, " flg ",  Hex(_pkt.flags()));
		print(output, " ci ",   _pkt.ciaddr());
		print(output, " yi ",   _pkt.yiaddr());
		print(output, " si ",   _pkt.siaddr());
		print(output, " gi ",   _pkt.giaddr());
		print(output, " ch ",   _pkt.client_mac());
		print(output, " srv ",  _pkt.server_name());
		print(output, " file ", _pkt.file());
		print(output, " mag ",  _pkt.magic_cookie());
		print(output, " opt");
		_pkt.for_each_option([&] (Dhcp_packet::Option &opt) {
			print(output, " ", opt);
		});
		break;

	case Packet_log_config::COMPACT:

		print(output, "\033[32mDHCP\033[0m");
		print(output, " ",   _pkt.op());
		print(output, " ",   _pkt.client_mac());
		print(output, " > ", _pkt.siaddr());
		break;

	case Packet_log_config::SHORT:

		print(output, "\033[32mDHCP\033[0m");
		break;

	default: ;
	}
}


void Dhcp_packet::Option::print(Output &output) const
{
	using Genode::print;

	print(output, _code, ":", _len);
	if (!len()) {
		return;
	}
	print(output, ":");
	for (unsigned j = 0; j < len(); j++) {
		print(output, Hex(_value[j], Hex::OMIT_PREFIX, Hex::PAD));
	}
}
