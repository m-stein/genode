/*
 * \brief  Wrapper for network packets to configure their print functionality
 * \author Martin Stein
 * \date   2017-09-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NET__PACKET_LOG_H_
#define _NET__PACKET_LOG_H_

/* Genode includes */
#include <base/stdint.h>

namespace Net {

	struct Packet_log_config;

	template <typename PKT>
	struct Packet_log;
}


/**
 * Configuration for the print functionality of network packets
 */
struct Net::Packet_log_config
{
	enum Enum : Genode::uint8_t { NONE, SHORT, COMPACT, COMPREHENSIVE };

	Enum eth, arp, ipv4, dhcp, udp, tcp;

	Packet_log_config(Enum def = COMPACT)
	: eth(def), arp(def), ipv4(def), dhcp(def), udp(def), tcp(def) { }

	Packet_log_config(Enum eth,
	                  Enum arp,
	                  Enum ipv4,
	                  Enum dhcp,
	                  Enum udp,
	                  Enum tcp)
	: eth(eth), arp(arp), ipv4(ipv4), dhcp(dhcp), udp(udp), tcp(tcp) { }
};


/**
 * Wrapper for network packets to configure their print functionality
 */
template <typename PKT>
class Net::Packet_log
{
	private:

		PKT               const &_pkt;
		Packet_log_config const &_cfg;

	public:

		Packet_log(PKT const &pkt, Packet_log_config const &cfg)
		: _pkt(pkt), _cfg(cfg) { }


		/*********
		 ** log **
		 *********/

		void print(Genode::Output &output) const;
};


namespace Net {

	/* getting instances via a function enables template-arg deduction */
	template <typename PKT>
	Packet_log<PKT> packet_log(PKT const &pkt, Packet_log_config const &cfg) {
		return Packet_log<PKT>(pkt, cfg); }
}

#endif /* _NET__PACKET_LOG_H_ */
