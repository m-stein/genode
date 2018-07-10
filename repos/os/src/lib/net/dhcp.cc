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


void Net::Dhcp_packet::print(Genode::Output &output) const
{
	char const *msg_type;
	try {
		switch (option<Dhcp_packet::Message_type_option>().value()) {
		case Message_type::DISCOVER : msg_type = "discover"; break;
		case Message_type::OFFER    : msg_type = "offer"   ; break;
		case Message_type::REQUEST  : msg_type = "request" ; break;
		case Message_type::DECLINE  : msg_type = "decline" ; break;
		case Message_type::ACK      : msg_type = "ack"     ; break;
		case Message_type::NAK      : msg_type = "nak"     ; break;
		case Message_type::RELEASE  : msg_type = "release" ; break;
		case Message_type::INFORM   : msg_type = "inform"  ; break;
		default                     : msg_type = "?"       ; break;
		}
	}
	catch (Dhcp_packet::Option_not_found exception) { msg_type = "?"; }

	Genode::print(output, "\033[32mDHCP\033[0m ", client_mac(),
	              " > ", siaddr(), " ", msg_type);
}
