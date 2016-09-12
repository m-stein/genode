/*
 * \brief  State tracking for UDP/TCP connections
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROXY_H_
#define _PROXY_H_

///* Genode includes */
//#include <timer_session/connection.h>
//#include <net/ipv4.h>
//#include <util/list.h>
//
//namespace Net {
//
//	class Tcp_packet;
//	class Udp_packet;
//	class Interface;
//	class Link;
//	class Tcp_link;
//	class Udp_link;
//	class Nat_client;
//	class Tcp_nat_link;
//	class Udp_nat_link;
//	class Port_allocator_guard;
//
//	using Tcp_nat_link_list = Genode::List<Tcp_nat_link>;
//	using Udp_nat_link_list = Genode::List<Udp_nat_link>;
//}
//
//
//class Net::Link
//{
//	protected:
//
//		using Signal_handler = Genode::Signal_handler<Link>;
//
//		Ipv4_address const     _client_ip;
//		Genode::uint16_t const _client_port;
//		Timer::Connection      _timer;
//		bool                   _del = false;
//		Signal_handler         _del_timeout;
//		unsigned const         _del_timeout_us;
//
//		void _del_timeout_handle() { _del = true; }
//
//	public:
//
//		Link(Genode::uint16_t    client_port,
//		     Ipv4_address        client_ip,
//		     Genode::Entrypoint &ep,
//		     unsigned const      rtt_sec);
//
//		bool matches_client(Ipv4_address     client_ip,
//		                    Genode::uint16_t client_port);
//
//		Genode::uint16_t client_port() const { return _client_port; }
//		Ipv4_address     client_ip()   const { return _client_ip; }
//		bool             del()         const { return _del; }
//};
//
//
//class Net::Tcp_link : public Link
//{
//	private:
//
//		bool _client_fin       = false;
//		bool _server_fin       = false;
//		bool _client_fin_acked = false;
//		bool _server_fin_acked = false;
//
//	public:
//
//		Tcp_link(Genode::uint16_t    client_port,
//		         Ipv4_address        client_ip,
//		         Genode::Entrypoint &ep,
//		         unsigned const      rtt_sec);
//
//		void matching_packet(Ipv4_packet *const ip, Tcp_packet *const tcp);
//};
//
//
//struct Net::Udp_link : public Link
//{
//	Udp_link(Genode::uint16_t    client_port,
//	         Ipv4_address        client_ip,
//	         Genode::Entrypoint &ep,
//	         unsigned const      rtt_sec);
//
//	void matching_packet();
//};
//
//
//class Net::Nat_client
//{
//	protected:
//
//		Ipv4_address const     _nat_client_ip;
//		Genode::uint16_t const _nat_client_port;
//		Interface             &_client_interface;
//		Port_allocator_guard  &_port_alloc;
//
//	public:
//
//		Nat_client(Ipv4_address          nat_client_ip,
//		           Interface            &client_interface,
//		           Port_allocator_guard &port_alloc);
//
//		~Nat_client();
//
//		bool matches_nat_client(Ipv4_address ip,
//		                        Genode::uint16_t port);
//
//
//		/***************
//		 ** Accessors **
//		 ***************/
//
//		Genode::uint16_t nat_client_port()  const { return _nat_client_port; }
//		Ipv4_address     nat_client_ip()    const { return _nat_client_ip; }
//		Interface       &client_interface() const { return _client_interface; }
//};
//
//
//struct Net::Tcp_nat_link : Tcp_link,
//                           Nat_client,
//                           Genode::List<Tcp_nat_link>::Element
//{
//	Tcp_nat_link(Ipv4_address           client_ip,
//	             Genode::uint16_t const client_port,
//	             Ipv4_address           nat_client_ip,
//	             Interface             &client_interface,
//	             Genode::Entrypoint    &ep,
//	             unsigned const         rtt_sec,
//	             Port_allocator_guard  &port_alloc);
//
//
//	/*********
//	 ** log **
//	 *********/
//
//	void print(Genode::Output &out) const;
//};
//
//
//struct Net::Udp_nat_link : Udp_link,
//                           Nat_client,
//                           Genode::List<Udp_nat_link>::Element
//{
//	Udp_nat_link(Ipv4_address           client_ip,
//	             Genode::uint16_t const client_port,
//	             Ipv4_address           nat_client_ip,
//	             Interface             &client_interface,
//	             Genode::Entrypoint    &ep,
//	             unsigned const         rtt_sec,
//	             Port_allocator_guard  &port_alloc);
//
//
//	/*********
//	 ** log **
//	 *********/
//
//	void print(Genode::Output &out) const;
//};

#endif /* _PROXY_H_ */
