/*
 * \brief  Downlink interface in form of a NIC session component
 * \author Martin Stein
 * \date   2017-03-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMPONENT_H_
#define _COMPONENT_H_

/* Genode includes */
#include <base/allocator_guard.h>
#include <root/component.h>
#include <nic/packet_allocator.h>
#include <nic_session/rpc_object.h>

/* local includes */
#include <interface.h>

namespace Net {

	class Communication_buffer;
	class Session_component_base;
	class Session_component;
	class Root;
	class Uplink;
}


class Net::Communication_buffer : public Genode::Ram_dataspace_capability
{
	private:

		Genode::Ram_session &_ram;

	public:

		Communication_buffer(Genode::Ram_session  &ram,
		                     Genode::size_t const  size);

		~Communication_buffer() { _ram.free(*this); }
};


class Net::Session_component_base
{
	protected:

		Genode::Allocator_guard _guarded_alloc;
		Nic::Packet_allocator   _range_alloc;
		Communication_buffer    _tx_buf;
		Communication_buffer    _rx_buf;

	public:

		Session_component_base(Genode::Allocator    &guarded_alloc_backing,
		                       Genode::size_t const  guarded_alloc_amount,
		                       Genode::Ram_session  &buf_ram,
		                       Genode::size_t const  tx_buf_size,
		                       Genode::size_t const  rx_buf_size);
};


class Net::Session_component : public Session_component_base,
                               public ::Nic::Session_rpc_object,
                               public Interface
{
	private:

		Mac_address _mac;


		/********************
		 ** Net::Interface **
		 ********************/

		Packet_stream_sink   &_sink()   { return *_tx.sink(); }
		Packet_stream_source &_source() { return *_rx.source(); }

	public:

		Session_component(Genode::Allocator    &alloc,
		                  Genode::size_t const  amount,
		                  Genode::Ram_session  &buf_ram,
		                  Genode::size_t const  tx_buf_size,
		                  Genode::size_t const  rx_buf_size,
		                  Genode::Region_map   &region_map,
		                  Uplink               &uplink,
		                  Genode::Xml_node      config,
		                  Genode::Timer        &timer,
		                  unsigned             &curr_time,
		                  Genode::Entrypoint   &ep);


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() { return _mac; }
		bool link_state();
		void link_state_sigh(Genode::Signal_context_capability sigh);
};


class Net::Root : public Genode::Root_component<Session_component,
                                                Genode::Single_client>
{
	private:

		Genode::Entrypoint  &_ep;
		Uplink              &_uplink;
		Genode::Ram_session &_buf_ram;
		Genode::Region_map  &_region_map;
		Genode::Xml_node     _config;
		Genode::Timer       &_timer;
		unsigned            &_curr_time;


		/********************
		 ** Root_component **
		 ********************/

		Session_component *_create_session(char const *args);

	public:

		Root(Genode::Entrypoint  &ep,
		     Genode::Allocator   &alloc,
		     Uplink              &uplink,
		     Genode::Ram_session &buf_ram,
		     Genode::Xml_node     config,
		     Genode::Timer       &timer,
		     unsigned            &curr_time,
		     Genode::Region_map  &region_map);
};

#endif /* _COMPONENT_H_ */
