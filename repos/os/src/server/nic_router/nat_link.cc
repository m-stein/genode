/*
 * \brief  UDP/TCP nat_link session
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

///* Genode includes */
//#include <net/tcp.h>
//#include <net/udp.h>
//#include <base/log.h>
//
///* local includes */
//#include <nat_link.h>
//#include <port_allocator.h>
//
//using namespace Net;
//using namespace Genode;
//
//
///**********
// ** Link **
// **********/
//
//Link::Link(uint16_t        client_port,
//           Ipv4_address    client_ip,
//           Entrypoint     &ep,
//           unsigned const  rtt_sec)
//:
//	_client_ip(client_ip),
//	_client_port(client_port),
//	_del_timeout(ep, *this, &Link::_del_timeout_handle),
//	_del_timeout_us(rtt_sec * 2 * 1000 * 1000)
//{ }
//
//
//bool Link::matches_client(Ipv4_address ip, uint16_t port)
//{
//	return ip == _client_ip && port == _client_port;
//}
//
//
///**************
// ** Tcp_link **
// **************/
//
//Tcp_link::Tcp_link(uint16_t        client_port,
//                   Ipv4_address    client_ip,
//                   Entrypoint     &ep,
//                   unsigned const  rtt_sec)
//:
//	Link(client_port, client_ip, ep, rtt_sec)
//{ }
//
//
//void Tcp_link::matching_packet(Ipv4_packet *const ip, Tcp_packet *const tcp)
//{
//	/* find out which side sent the packet */
//	bool from_client;
//	if (tcp->src_port() == _client_port) { from_client = true; }
//	else { from_client = false; }
//
//	/* Remember FIN packets and which side sent them */
//	if (tcp->fin()) {
//		if (from_client) { _client_fin = true; }
//		else { _server_fin  = true; }
//	}
//	/* look for packets that ACK a previous FIN and remember those ACKs */
//	if (tcp->ack()) {
//		if (from_client  && _server_fin) { _server_fin_acked = true; }
//		if (!from_client && _client_fin) { _client_fin_acked = true; }
//
//		/* if both sides sent a FIN and got ACKed, init delayed destruction */
//		if (_server_fin_acked && _client_fin_acked) {
//			_timer.sigh(_del_timeout);
//			_timer.trigger_once(_del_timeout_us);
//		}
//	}
//}
//
//
///**************
// ** Udp_link **
// **************/
//
//Udp_link::Udp_link(uint16_t        client_port,
//                   Ipv4_address    client_ip,
//                   Entrypoint     &ep,
//                   unsigned const  rtt_sec)
//:
//	Link(client_port, client_ip, ep, rtt_sec)
//{
//	_timer.sigh(_del_timeout);
//	_timer.trigger_once(_del_timeout_us);
//}
//
//
//void Udp_link::matching_packet()
//{
//	_timer.trigger_once(_del_timeout_us);
//}
//
//
///****************
// ** Nat_client **
// ****************/
//
//Nat_client::Nat_client(Ipv4_address          nat_client_ip,
//                       Interface            &client_interface,
//                       Port_allocator_guard &port_alloc)
//:
//	_nat_client_ip(nat_client_ip),
//	_nat_client_port(port_alloc.alloc()),
//	_client_interface(client_interface),
//	_port_alloc(port_alloc)
//{ }
//
//
//Nat_client::~Nat_client()
//{
//	_port_alloc.free(_nat_client_port);
//}
//
//
//bool Nat_client::matches_nat_client(Ipv4_address ip, uint16_t port)
//{
//	return ip == _nat_client_ip && port == _nat_client_port;
//}
//
//
///******************
// ** Tcp_nat_link **
// ******************/
//
//Tcp_nat_link::Tcp_nat_link(Ipv4_address          client_ip,
//                           uint16_t const        client_port,
//                           Ipv4_address          nat_client_ip,
//                           Interface            &client_interface,
//                           Entrypoint           &ep,
//                           unsigned const        rtt_sec,
//                           Port_allocator_guard &port_alloc)
//:
//	Tcp_link(client_port, client_ip, ep, rtt_sec),
//	Nat_client(nat_client_ip, client_interface, port_alloc)
//{ }
//
//
//void Tcp_nat_link::print(Output &out) const
//{
//	Genode::print(out, _client_ip, ":", _client_port, " -> ",
//	              _nat_client_ip, ":", _nat_client_port);
//}
//
//
///******************
// ** Udp_nat_link **
// ******************/
//
//Udp_nat_link::Udp_nat_link(Ipv4_address          client_ip,
//                           uint16_t const        client_port,
//                           Ipv4_address          nat_client_ip,
//                           Interface            &client_interface,
//                           Entrypoint           &ep,
//                           unsigned const        rtt_sec,
//                           Port_allocator_guard &port_alloc)
//:
//	Udp_link(client_port, client_ip, ep, rtt_sec),
//	Nat_client(nat_client_ip, client_interface, port_alloc)
//{ }
//
//
//void Udp_nat_link::print(Output &out) const
//{
//	Genode::print(out, _client_ip, ":", _client_port, " -> ",
//	              _nat_client_ip, ":", _nat_client_port);
//}
