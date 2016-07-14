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

#ifndef _PACKET_HANDLER_H_
#define _PACKET_HANDLER_H_

/* Genode */
#include <base/semaphore.h>
#include <base/thread.h>
#include <nic_session/connection.h>
#include <os/server.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <os/session_policy.h>

#include <vlan.h>
#include <port_allocator.h>

namespace Net {

	class Arp_packet;
	class Packet_handler;

	using ::Nic::Packet_stream_sink;
	using ::Nic::Packet_stream_source;
}

/**
 * Generic packet handler used as base for NIC and client packet handlers.
 */
class Net::Packet_handler : public Interface_node
{
	protected:

		using size_t = Genode::size_t;

	private:

		Packet_descriptor    _packet;
		Net::Vlan &          _vlan;
		Genode::Entrypoint & _ep;

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

		/**
		 * the link-state of changed
		 */
		void _link_state(unsigned);


		void _handle_arp_reply(Arp_packet * const arp);

		void _handle_arp_request(Ethernet_frame * const eth,
		                         size_t const eth_size,
		                         Arp_packet * const arp);

		Arp_waiter * _new_arp_node(Arp_waiter * arp_waiter, Arp_node * arp_node);

		void _remove_arp_waiter(Arp_waiter * arp_waiter);

		/*
		 * Handle an ARP packet
		 *
		 * \param eth   ethernet frame containing the ARP packet.
		 * \param size  ethernet frame's size.
		 */
		bool _handle_arp(Ethernet_frame *eth, size_t size);

		bool _handle_ip(Ethernet_frame * eth, size_t eth_size,
		                bool & ack, Packet_descriptor * p);

		void _handle_udp_to_others(Ethernet_frame * eth, size_t eth_size,
		                           Ipv4_packet * ip, size_t ip_size,
		                           bool & ack, Packet_descriptor * p);

		void _handle_udp_to_nat(Ethernet_frame * eth, size_t eth_size,
		                        Ipv4_packet * ip, size_t ip_size,
		                        bool & ack, Packet_descriptor * p);

		void _handle_udp(Ethernet_frame * eth, size_t eth_size,
		                 Ipv4_packet * ip, size_t ip_size, bool & ack,
		                 Packet_descriptor * p);


		void _handle_tcp_to_others(Ethernet_frame * eth, size_t eth_size,
		                           Ipv4_packet * ip, size_t ip_size,
		                           bool & ack, Packet_descriptor * p);

		void _handle_tcp_to_nat(Ethernet_frame * eth, size_t eth_size,
		                        Ipv4_packet * ip, size_t ip_size,
		                        bool & ack, Packet_descriptor * p);

		void _handle_tcp(Ethernet_frame * eth, size_t eth_size,
		                 Ipv4_packet * ip, size_t ip_size, bool & ack,
		                 Packet_descriptor * p);

		void _apply_proxy(
			Ipv4_packet * ip, size_t ip_size, Ipv4_address proxy_ip);


		void _handle_to_others_known_arp(
			Ethernet_frame * const eth, size_t const eth_size,
			Ipv4_packet * const ip, size_t const ip_size,
			Arp_node * const arp_node, Packet_handler * handler);

		void _handle_to_others(
			Ethernet_frame * eth, size_t eth_size, Ipv4_packet * ip,
			size_t ip_size, bool & ack, Packet_descriptor * p);


		void _handle_to_others_unknown_arp(
			Ethernet_frame * eth, size_t eth_size, Ipv4_address ip_addr,
			Packet_handler * handler, bool & ack, Packet_descriptor * p);

		Proxy_role * _new_proxy_role(
			unsigned const client_port, Ipv4_address client_ip,
			Ipv4_address proxy_ip);

		void _too_many_proxy_roles();

		void _delete_proxy_role(Proxy_role * const role);

		bool _chk_delete_proxy_role(Proxy_role * & role);

	protected:

		Packet_handler * _ip_routing(Ipv4_address & ip_addr, Ipv4_packet * ip);

		Genode::Signal_rpc_member<Packet_handler> _sink_ack;
		Genode::Signal_rpc_member<Packet_handler> _sink_submit;
		Genode::Signal_rpc_member<Packet_handler> _source_ack;
		Genode::Signal_rpc_member<Packet_handler> _source_submit;
		Genode::Signal_rpc_member<Packet_handler> _client_link_state;

		Mac_address         _nat_mac;
		Ipv4_address        _nat_ip;
		Genode::Allocator * _allocator;
		Genode::Session_policy _policy;
		bool const          _proxy;
		unsigned const      _proxy_ports;
		unsigned            _proxy_ports_used;
		Port_allocator &    _port_alloc;

	public:

		void arp_broadcast(Ipv4_address ip_addr);

		Mac_address  nat_mac()    {return _nat_mac;}
		Ipv4_address nat_ip()     {return _nat_ip;}

		Packet_handler(
			Server::Entrypoint & ep, Vlan & vlan, Mac_address nat_mac,
			Ipv4_address nat_ip, Genode::Allocator * allocator,
			Genode::Session_label & label, Port_allocator & port_alloc);

		virtual Packet_stream_sink< ::Nic::Session::Policy>   * sink()   = 0;
		virtual Packet_stream_source< ::Nic::Session::Policy> * source() = 0;

		Net::Vlan & vlan() { return _vlan; }

		/**
		 * Broadcasts ethernet frame to all clients,
		 * as long as its really a broadcast packtet.
		 *
		 * \param eth   ethernet frame to send.
		 * \param size  ethernet frame's size.
		 */
		void inline broadcast_to_clients(Ethernet_frame *eth,
		                                 size_t size);

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

		/*
		 * Finalize handling of ethernet frame.
		 *
		 * \param eth   ethernet frame to handle.
		 * \param size  ethernet frame's size.
		 */
		virtual void finalize_packet(Ethernet_frame *eth,
		                             size_t size) = 0;

		Genode::Allocator * allocator() const { return _allocator; }
};

#endif /* _PACKET_HANDLER_H_ */
