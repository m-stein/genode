/*
 * \brief  A net interface in form of a signal-driven NIC-packet handler
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

/* Genode */
#include <base/semaphore.h>
#include <base/thread.h>
#include <nic_session/connection.h>
#include <os/server.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <os/session_policy.h>
#include <util/avl_string.h>
#include <util/avl_tree.h>

/* local includes */
#include <port_allocator.h>
#include <arp_waiter.h>
#include <ip_route.h>
#include <proxy.h>

namespace Net {

	class Arp_packet;
	class Arp_cache;
	class Interface;
	class Interface_tree;
	using Interface_list = Genode::List<Interface>;
	using ::Nic::Packet_stream_sink;
	using ::Nic::Packet_stream_source;
}

class Net::Interface : public Interface_label,
                       public Genode::Avl_string_base
{
	protected:

		using Signal_rpc_member = Genode::Signal_rpc_member<Interface>;

		Signal_rpc_member _sink_ack;
		Signal_rpc_member _sink_submit;
		Signal_rpc_member _source_ack;
		Signal_rpc_member _source_submit;

	private:

		Packet_descriptor       _packet;
		Genode::Entrypoint     &_ep;
		Ip_route_list           _ip_routes;
		Mac_address const       _nat_mac;
		Ipv4_address const      _nat_ip;
		Mac_address const       _mac;
		Ipv4_address const      _ip;
		Genode::Allocator      &_allocator;
		Genode::Session_policy  _policy;
		unsigned const          _tcp_proxy;
		unsigned                _tcp_proxy_used;
		Tcp_proxy_list         &_tcp_proxys;
		Port_allocator         &_tcp_port_alloc;
		unsigned const          _udp_proxy;
		unsigned                _udp_proxy_used;
		Udp_proxy_list         &_udp_proxys;
		Port_allocator         &_udp_port_alloc;
		unsigned const          _rtt_sec;
		Interface_tree         &_interface_tree;
		Arp_cache              &_arp_cache;
		Arp_waiter_list        &_arp_waiters;

		void _read_route(Genode::Xml_node &route_xn);

		Tcp_proxy *_find_tcp_proxy_by_client(Ipv4_address ip,
		                                     Genode::uint16_t port);

		Udp_proxy *_find_udp_proxy_by_client(Ipv4_address ip,
		                                     Genode::uint16_t port);

		Interface *_tlp_proxy_route(Genode::uint8_t tlp, void * ptr,
		                            Genode::uint16_t & dst_port,
		                            Ipv4_packet * ip, Ipv4_address & to,
		                            Ipv4_address & via);

		void tlp_port_proxy(Genode::uint8_t tlp, void * tlp_ptr,
		                    Ipv4_packet * ip, Ipv4_address client_ip,
		                    Genode::uint16_t src_port);

		Tcp_proxy * _find_tcp_proxy_by_proxy(Ipv4_address ip,
		                                     Genode::uint16_t port);

		Udp_proxy * _find_udp_proxy_by_proxy(
			Ipv4_address ip, Genode::uint16_t port);

		void _handle_arp_reply(Arp_packet * const arp);

		void _handle_arp_request(Ethernet_frame * const eth,
		                         Genode::size_t const eth_size,
		                         Arp_packet * const arp);

		Arp_waiter * _new_arp_entry(Arp_waiter * arp_waiter,
		                                  Arp_cache_entry * entry);

		void _remove_arp_waiter(Arp_waiter * arp_waiter);

		/*
		 * Handle an ARP packet
		 *
		 * \param eth   ethernet frame containing the ARP packet.
		 * \param size  ethernet frame's size.
		 */
		void _handle_arp(Ethernet_frame *eth, Genode::size_t size);

		void _handle_ip(Ethernet_frame * eth, Genode::size_t eth_size,
		                bool & ack_packet, Packet_descriptor * packet);

		Tcp_proxy * _new_tcp_proxy(
			unsigned const client_port, Ipv4_address client_ip,
			Ipv4_address proxy_ip);

		Udp_proxy * _new_udp_proxy(
			unsigned const client_port, Ipv4_address client_ip,
			Ipv4_address proxy_ip);

		void _delete_tcp_proxy(Tcp_proxy * const role);

		bool _chk_delete_tcp_proxy(Tcp_proxy * & role);

		void _delete_udp_proxy(Udp_proxy * const role);

		bool _chk_delete_udp_proxy(Udp_proxy * & role);

		bool _tlp_proxy(Genode::uint8_t tlp);


		/***********************************
		 ** Packet-stream signal handlers **
		 ***********************************/

		void _ready_to_submit(unsigned);
		void _ack_avail(unsigned) { }
		void _ready_to_ack(unsigned);
		void _packet_avail(unsigned) { }

	public:

		void arp_broadcast(Ipv4_address ip_addr);

		Mac_address  nat_mac()    {return _nat_mac;}
		Ipv4_address nat_ip()     {return _nat_ip;}
		Mac_address  mac_addr()    {return _mac;}
		Ipv4_address ip_addr()     {return _ip;}

		Interface(Server::Entrypoint    &ep,
		          Mac_address            nat_mac,
		          Ipv4_address           nat_ip,
		          Genode::Allocator     &allocator,
		          Genode::Session_label &label,
		          Port_allocator        &tcp_port_alloc,
		          Port_allocator        &udp_port_alloc,
		          Mac_address            mac,
		          Tcp_proxy_list        &tcp_proxys,
		          Udp_proxy_list        &udp_proxys,
		          unsigned               rtt_sec,
		          Interface_tree        &interface_tree,
		          Arp_cache             &arp_cache,
		          Arp_waiter_list       &arp_waiters);

		~Interface();

		virtual Packet_stream_sink< ::Nic::Session::Policy>   * sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> * source() = 0;

		Ip_route_list * routes() { return &_ip_routes; }

		/**
		 * Send ethernet frame
		 *
		 * \param eth   ethernet frame to send.
		 * \param size  ethernet frame's size.
		 */
		void send(Ethernet_frame *eth, Genode::size_t size);

		/**
		 * Handle an ethernet packet
		 *
		 * \param src   ethernet frame's address
		 * \param size  ethernet frame's size.
		 */
		void handle_ethernet(void* src, Genode::size_t size, bool & ack, Packet_descriptor * p);

		void continue_handle_ethernet(void* src, Genode::size_t size, Packet_descriptor * p);

		Genode::Allocator & allocator() const { return _allocator; }
};

struct Net::Interface_tree : Genode::Avl_tree<Genode::Avl_string_base>
{
	Interface * find_by_label(char const * label);
};

#endif /* _INTERFACE_H_ */
