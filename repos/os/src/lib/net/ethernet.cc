/*
 * \brief  Ethernet protocol
 * \author Stefan Kalkowski
 * \date   2010-08-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <net/ethernet.h>


#include <util/token.h>
#include <util/string.h>

using namespace Net;

const Net::Mac_address Net::Ethernet_frame::BROADCAST(0xFF);

struct Scanner_policy_number
{
		static bool identifier_char(char c, unsigned  i ) {
			return Genode::is_digit(c) && c !=':'; }
};

typedef ::Genode::Token<Scanner_policy_number> Token;

Net::Mac_address Net::mac_from_string(const char * mac) {

	Mac_address   mac_addr;
	Token         t(mac);
	char          tmpstr[3];
	int           cnt = 0;
	unsigned char ipb[6] = {0};

	while(t) {
		if (t.type() == Token::WHITESPACE || t[0] == ':') {
			t = t.next();
			continue;
		}
		t.string(tmpstr, sizeof(tmpstr));

		unsigned long tmpc = 0;
		Genode::ascii_to(tmpstr, tmpc);
		ipb[cnt] = tmpc & 0xFF;
		t = t.next();

		if (cnt == 6)
			break;
		cnt++;
	}

	if (cnt == 6) {
		mac_addr.addr[0] = ipb[0];
		mac_addr.addr[1] = ipb[1];
		mac_addr.addr[2] = ipb[2];
		mac_addr.addr[3] = ipb[3];
		mac_addr.addr[4] = ipb[4];
		mac_addr.addr[5] = ipb[5];
	}

	return mac_addr;
}
