/*
 * \brief  Signal driven NIC packet handler
 * \author Stefan Kalkowski
 * \date   2010-08-18
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

/* local includes */
#include <port_allocator.h>
#include <arp_waiter.h>
#include <ip_route.h>
#include <proxy_role.h>
#include <avl_safe.h>

namespace Net
{
	class Arp_packet;
	class Vlan;
	class Interface;
	class Interface_tree;
	using Interface_list = List_safe<Interface>;
	using ::Nic::Packet_stream_sink;
	using ::Nic::Packet_stream_source;
}

class Net::Interface
:
	public Interface_label,
	public Genode::Avl_string_base
{
	protected:

		using size_t = Genode::size_t;

	private:

		Packet_descriptor    _packet;
		Net::Vlan &          _vlan;
		Genode::Entrypoint & _ep;
		Ip_route_list        _ip_routes;

		void _read_route(Genode::Xml_node & route_xn);

		/**
		 * submit queue not empty anymore
		 */
		void _ready_to_submit(unsigned);

		/**
		 * acknoledgement queue not full anymore
		 *
		 * TODO: by now, we assume ACK and SUBMIT queue to be equally
		 *       dimensioned. That's why we ignore this signal by now.
		 */
		void _ack_avail(unsigned) { }

		/**
		 * acknoledgement queue not empty anymore
		 */
		void _ready_to_ack(unsigned);

		/**
		 * submit queue not full anymore
		 *
		 * TODO: by now, we just drop packets that cannot be transferred
		 *       to the other side, that's why we ignore this signal.
		 */
		void _packet_avail(unsigned) { }

		Tcp_proxy_role * _find_tcp_proxy_role_by_client(Ipv4_address ip, Genode::uint16_t port);
		Udp_proxy_role * _find_udp_proxy_role_by_client(Ipv4_address ip, Genode::uint16_t port);

		Interface * _tlp_proxy_route(
			Genode::uint8_t tlp, void * ptr, Genode::uint16_t & dst_port,
			Ipv4_packet * ip, Ipv4_address & to, Ipv4_address & via);

		void tlp_port_proxy(
			Genode::uint8_t tlp, void * tlp_ptr, Ipv4_packet * ip,
			Ipv4_address client_ip, Genode::uint16_t src_port);

		Tcp_proxy_role * _find_tcp_proxy_role_by_proxy(
			Ipv4_address ip, Genode::uint16_t port);

		Udp_proxy_role * _find_udp_proxy_role_by_proxy(
			Ipv4_address ip, Genode::uint16_t port);

		void _handle_arp_reply(Arp_packet * const arp);

		void _handle_arp_request(Ethernet_frame * const eth,
		                         size_t const eth_size,
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
		void _handle_arp(Ethernet_frame *eth, size_t size);

		void _handle_ip(Ethernet_frame * eth, size_t eth_size,
		                bool & ack_packet, Packet_descriptor * packet);

		Tcp_proxy_role * _new_tcp_proxy_role(
			unsigned const client_port, Ipv4_address client_ip,
			Ipv4_address proxy_ip);

		Udp_proxy_role * _new_udp_proxy_role(
			unsigned const client_port, Ipv4_address client_ip,
			Ipv4_address proxy_ip);

		void _delete_tcp_proxy_role(Tcp_proxy_role * const role);

		bool _chk_delete_tcp_proxy_role(Tcp_proxy_role * & role);

		void _delete_udp_proxy_role(Udp_proxy_role * const role);

		bool _chk_delete_udp_proxy_role(Udp_proxy_role * & role);

		bool _tlp_proxy(Genode::uint8_t tlp);

	protected:

		Genode::Signal_rpc_member<Interface> _sink_ack;
		Genode::Signal_rpc_member<Interface> _sink_submit;
		Genode::Signal_rpc_member<Interface> _source_ack;
		Genode::Signal_rpc_member<Interface> _source_submit;

		Mac_address         _nat_mac;
		Ipv4_address        _nat_ip;
		Mac_address _mac;
		Ipv4_address _ip;
		Genode::Allocator * _allocator;
		Genode::Session_policy _policy;
		bool                _tcp_proxy;
		unsigned            _tcp_proxy_ports;
		unsigned            _tcp_proxy_ports_used;
		Port_allocator &    _tcp_port_alloc;

		bool                _udp_proxy;
		unsigned            _udp_proxy_ports;
		unsigned            _udp_proxy_ports_used;
		Port_allocator &    _udp_port_alloc;

	public:

		void arp_broadcast(Ipv4_address ip_addr);

		Mac_address  nat_mac()    {return _nat_mac;}
		Ipv4_address nat_ip()     {return _nat_ip;}
		Mac_address  mac_addr()    {return _mac;}
		Ipv4_address ip_addr()     {return _ip;}

		Interface(
			Server::Entrypoint & ep, Vlan & vlan, Mac_address nat_mac,
			Ipv4_address nat_ip, Genode::Allocator * allocator,
			Genode::Session_label & label, Port_allocator & tcp_port_alloc,
			Port_allocator & udp_port_alloc,
			Mac_address mac);

		~Interface();

		virtual Packet_stream_sink< ::Nic::Session::Policy>   * sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> * source() = 0;

		Net::Vlan & vlan() { return _vlan; }

		Ip_route_list * routes() { return &_ip_routes; }

		/**
		 * Send ethernet frame
		 *
		 * \param eth   ethernet frame to send.
		 * \param size  ethernet frame's size.
		 */
		void send(Ethernet_frame *eth, size_t size);

		/**
		 * Handle an ethernet packet
		 *
		 * \param src   ethernet frame's address
		 * \param size  ethernet frame's size.
		 */
		void handle_ethernet(void* src, size_t size, bool & ack, Packet_descriptor * p);

		void continue_handle_ethernet(void* src, size_t size, Packet_descriptor * p);

		Genode::Allocator * allocator() const { return _allocator; }
};

class Net::Interface_tree
:
	public Avl_tree_safe<Genode::Avl_string_base>
{
	public:

		Interface * find_by_label(char const * label);
};

#endif /* _INTERFACE_H_ */
