/*
 * \brief  DHCP related definitions
 * \author Stefan Kalkowski
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
#include <base/output.h>

using namespace Genode;


void Net::Dhcp_packet::print(Genode::Output &output) const
{
	Genode::print(output, "\033[32mDHCP\033[0m");
	Genode::print(output, " op ",  op());
	Genode::print(output, " ht ",  htype());
	Genode::print(output, " hl ",  hlen());
	Genode::print(output, " ho ",  hops());
	Genode::print(output, " xi ",  xid());
	Genode::print(output, " se ",  secs());
	Genode::print(output, " fl ",  flags());
	Genode::print(output, " ci ",  ciaddr());
	Genode::print(output, " yi ",  yiaddr());
	Genode::print(output, " si ",  siaddr());
	Genode::print(output, " gi ",  giaddr());
	Genode::print(output, " ch ",  client_mac());
	Genode::print(output, " sn ",  server_name());
	Genode::print(output, " fi ",  file());
	Genode::print(output, " ma ",  magic_cookie());
	Genode::print(output, " os ");
	for (unsigned i = 0; ; ) {
		Option &opt = *(Option*)&_opts[i];
		Genode::print(output, opt.code(), ":", opt.length(), ":");
		if (opt.code() == 0 || opt.code() == END) { return; }
		for (unsigned j = 0; j < opt.length(); j++) {
			Genode::print(output, Hex(((uint8_t*)opt.value())[j], Hex::OMIT_PREFIX));
		}
		Genode::print(output, " ");
		i += 2 + opt.length();
	}
}
