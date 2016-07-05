/*
 * \brief  Proxy-ARP session and root component
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <base/lock.h>
#include <root/component.h>
#include <util/arg_string.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>
#include <nic_session/connection.h>
#include <net/ipv4.h>
#include <base/allocator_guard.h>
#include <os/session_policy.h>

/* local includes */
#include <address_node.h>
#include <mac.h>
#include <uplink.h>
#include <packet_handler.h>

namespace Net
{
	class Guarded_range_allocator;
	class Communication_buffer;
	class Tx_rx_communication_buffers;
	class Session_component;
	class Root;
}

class Net::Guarded_range_allocator
{
	private:

		using Allocator       = Genode::Allocator;
		using size_t          = Genode::size_t;
		using Allocator_guard = Genode::Allocator_guard;
		using Range_allocator = Genode::Range_allocator;

		Genode::Allocator_guard _guarded_alloc;
		Nic::Packet_allocator   _range_alloc;

	public:

		Guarded_range_allocator(Allocator * backing_store, size_t amount)
		:
			_guarded_alloc(backing_store, amount),
			_range_alloc(&_guarded_alloc)
		{ }

		Allocator_guard * guarded_allocator() { return &_guarded_alloc; }

		Range_allocator * range_allocator() {
			return static_cast<Range_allocator *>(&_range_alloc); }
};


class Net::Communication_buffer : Genode::Ram_dataspace_capability
{
	private:

		using size_t                   = Genode::size_t;
		using Ram_dataspace_capability = Genode::Ram_dataspace_capability;
		using Dataspace_capability     = Genode::Dataspace_capability;

	public:

		Communication_buffer(size_t size)
		: Ram_dataspace_capability(Genode::env()->ram_session()->alloc(size))
		{ }

		~Communication_buffer() { Genode::env()->ram_session()->free(*this); }

		Dataspace_capability dataspace() { return * this; }
};


class Net::Tx_rx_communication_buffers
{
	private:

		using size_t               = Genode::size_t;
		using Dataspace_capability = Genode::Dataspace_capability;

		Communication_buffer _tx_buf, _rx_buf;

	public:

		Tx_rx_communication_buffers(size_t tx_size, size_t rx_size)
		: _tx_buf(tx_size), _rx_buf(rx_size) { }

		Dataspace_capability tx_ds() { return _tx_buf.dataspace(); }
		Dataspace_capability rx_ds() { return _rx_buf.dataspace(); }
};

/**
 * Nic-session component class
 *
 * We must inherit here from Guarded_range_allocator, although aggregation
 * would be more convinient, because the range-allocator needs to be initialized
 * before base-class Session_rpc_object.
 */
class Net::Session_component : public  Guarded_range_allocator,
                          private Tx_rx_communication_buffers,
                          public  ::Nic::Session_rpc_object,
                          public  Packet_handler
{
	private:

		using uint8_t =                   Genode::uint8_t;
		using size_t =                    Genode::size_t;
		using Signal_context_capability = Genode::Signal_context_capability;
		using Allocator =                 Genode::Allocator;
		using Signal_transmitter =        Genode::Signal_transmitter;

		Mac_address_node            _mac_node;
		Ipv4_address_node         * _ipv4_node;
		Port_node                 * _port_node;
		Uplink                     &_uplink;
		Signal_context_capability   _link_state_sigh;

		void _free_ipv4_node();
		void _free_port_node();

		void _arp_broadcast(Packet_handler * handler,
		                    Ipv4_address ip_addr);

	public:

		/**
		 * Constructor
		 *
		 * \param allocator    backing store for guarded allocator
		 * \param amount       amount of memory managed by guarded allocator
		 * \param tx_buf_size  buffer size for tx channel
		 * \param rx_buf_size  buffer size for rx channel
		 * \param vmac         virtual mac address
		 * \param ep           entry point used for packet stream
		 */
		Session_component(Allocator          * allocator,
		                  size_t               amount,
		                  size_t               tx_buf_size,
		                  size_t               rx_buf_size,
		                  Mac_address          vmac,
		                  Server::Entrypoint & ep,
		                  Uplink             & uplink,
		                  char               * ip_addr,
					Mac_address nat_mac,
		                  Ipv4_address nat_ip,
		                  unsigned             port, char const * label);

		~Session_component();

		Mac_address mac_address() { return _mac_node.addr(); }

		Ipv4_address ipv4_address()
		{
			if (!_ipv4_node) { return Ipv4_address((uint8_t)0); }
			return Ipv4_address(_ipv4_node->addr());
		}

		void link_state_changed()
		{
			if (!_link_state_sigh.valid()) { return; }
			Signal_transmitter(_link_state_sigh).submit();
		}

		void set_ipv4_address(Ipv4_address ip_addr);

		void set_port(unsigned port);

		/****************************************
		 ** Nic::Driver notification interface **
		 ****************************************/

		bool link_state();

		void link_state_sigh(Signal_context_capability sigh) {
			_link_state_sigh = sigh; }

		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink<Nic::Session::Policy>   * sink()   { return _tx.sink(); }
		Packet_stream_source<Nic::Session::Policy> * source() { return _rx.source(); }

		void finalize_packet(Ethernet_frame *eth, size_t size);
};


/*
 * Root component, handling new session requests.
 */
class Net::Root : public Genode::Root_component<Session_component>
{
	private:

		enum { verbose = 1 };

		Mac_allocator       _mac_alloc;
		Server::Entrypoint &_ep;
		Uplink             &_uplink;
		Mac_address         _nat_mac;

	protected:

		enum { MAX_IP_ADDR_LENGTH  = 16, };
		char ip_addr[MAX_IP_ADDR_LENGTH];
		char nat_ip_addr[MAX_IP_ADDR_LENGTH];

		Session_component *_create_session(const char *args);

	public:

		Root(Server::Entrypoint &ep,
		     Uplink             &uplink,
		     Genode::Allocator  *md_alloc, Mac_address nat_mac);
};

#endif /* _COMPONENT_H_ */
