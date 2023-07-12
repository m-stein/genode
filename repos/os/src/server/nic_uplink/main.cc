/*
 * \brief  Component construct and main component object
 * \author Martin Stein
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/heap.h>
#include <util/xml_generator.h>
#include <root/component.h>

namespace Net {

	class Quota;
}

struct Net::Quota
{
	Genode::size_t ram { 0 };
	Genode::size_t cap { 0 };
};

/* nic_router includes */
#include <session_env.h>
#include <communication_buffer.h>

/* os includes */
#include <net/ethernet.h>
#include <net/internet_checksum.h>
#include <nic/packet_allocator.h>
#include <uplink_session/rpc_object.h>
#include <nic_session/rpc_object.h>

using namespace Genode;
using namespace Net;

namespace Net {

	enum { PKT_STREAM_QUEUE_SIZE = 1024 };

	using Packet_descriptor = Genode::Packet_descriptor;
	using Packet_stream_policy = Genode::Packet_stream_policy<Packet_descriptor, PKT_STREAM_QUEUE_SIZE, PKT_STREAM_QUEUE_SIZE, char>;
	using Packet_stream_sink = Genode::Packet_stream_sink<Packet_stream_policy>;
	using Packet_stream_source = Genode::Packet_stream_source<Packet_stream_policy>;

	class Network_interface;
	class Uplink_session_component_base;
	class Uplink_session_component;
	class Uplink_session_root;
	class Nic_session_component_base;
	class Nic_session_component;
	class Nic_session_root;
}

namespace Nic_uplink {

	class Main;
}


class Net::Network_interface
{
	private:

		Packet_stream_sink &_sink;
		Packet_stream_source &_source;

	public:

		Network_interface(Packet_stream_sink &sink,
		                  Packet_stream_source &source)
		:
			_sink { sink },
			_source { source }
		{ }

		virtual ~Network_interface() { }

		template <typename GENERATE_PKT>
		void send_packet(size_t pkt_size, GENERATE_PKT && generate_pkt)
		{
			if (!_source.ready_to_submit()) {
				error("failed to send packet");
				return;
			}
			_source.alloc_packet_attempt(pkt_size).with_result(
				[&] (Packet_descriptor pkt)
				{
					void *pkt_base { _source.packet_content(pkt) };
					generate_pkt(Byte_range_ptr { (char *)pkt_base, pkt_size });
					Size_guard size_guard1(pkt_size);
					log("snd ", Ethernet_frame::cast_from(pkt_base, size_guard1));
					_source.try_submit_packet(pkt);
				},
				[&] (Packet_stream_source::Alloc_packet_error)
				{
					error("failed to alloc packet");
				}
			);
		}

		template <typename HANDLE_PKT>
		void handle_received_packets(HANDLE_PKT && handle_pkt) const
		{
			while (_source.ack_avail()) {
				_source.release_packet(_source.try_get_acked_packet());
			}
			while (_sink.packet_avail()) {
				Packet_descriptor const pkt { _sink.get_packet() };
				handle_pkt(Byte_range_ptr { _sink.packet_content(pkt), pkt.size() });
				if (!_sink.try_ack_packet(pkt))
					error("failed to ack packet");
			}
			_source.wakeup();
			_sink.wakeup();
		}
};


class Net::Uplink_session_component_base
{
	protected:

		Session_env &_session_env;
		Heap _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer _tx_buf;
		Communication_buffer _rx_buf;

	public:

		Uplink_session_component_base(Session_env &session_env,
		                              size_t const tx_buf_size,
		                              size_t const rx_buf_size);
};


class Net::Uplink_session_component
:
	private Uplink_session_component_base,
	public ::Uplink::Session_rpc_object
{
	private:

		Ram_dataspace_capability const _ram_ds;
		Network_interface _net_if { *_tx.sink(), *_rx.source() };
		Signal_handler<Uplink_session_component> _pkt_stream_signal_handler;

		void _handle_pkt_stream_signal();

	public:

		Uplink_session_component(Session_env &session_env,
		                         size_t const tx_buf_size,
		                         size_t const rx_buf_size,
		                         Mac_address const mac,
		                         Ram_dataspace_capability const ram_ds);


		/***************
		 ** Accessors **
		 ***************/

		Ram_dataspace_capability ram_ds() const { return _ram_ds; };
		Session_env const &session_env() const { return _session_env; };
};


class Net::Uplink_session_root
:
	public Root_component<Uplink_session_component>
{
	private:

		Env &_env;
		Quota &_shared_quota;


		/********************
		 ** Root_component **
		 ********************/

		Uplink_session_component *_create_session(char const *args) override;
		void _destroy_session(Uplink_session_component *session) override;

	public:

		Uplink_session_root(Env &env,
		                    Allocator &alloc,
		                    Quota &shared_quota);
};


class Net::Nic_session_component_base
{
	protected:

		Session_env &_session_env;
		Heap _alloc;
		Nic::Packet_allocator _packet_alloc;
		Communication_buffer _tx_buf;
		Communication_buffer _rx_buf;

	public:

		Nic_session_component_base(Session_env &session_env,
		                           size_t const tx_buf_size,
		                           size_t const rx_buf_size);
};


class Net::Nic_session_component
:
	private Nic_session_component_base,
	public ::Nic::Session_rpc_object
{
	private:

		Ram_dataspace_capability const _ram_ds;
		Signal_handler<Nic_session_component> _pkt_stream_signal_handler;
		Signal_context_capability _link_state_sigh { };

	public:

		Nic_session_component(Session_env &session_env,
		                      size_t const tx_buf_size,
		                      size_t const rx_buf_size,
		                      Ram_dataspace_capability const ram_ds);

		void _handle_pkt_stream_signal();


		/******************
		 ** Nic::Session **
		 ******************/

		Mac_address mac_address() override;
		bool link_state() override;
		void link_state_sigh(Signal_context_capability sigh) override;


		/***************
		 ** Accessors **
		 ***************/

		Ram_dataspace_capability ram_ds() const { return _ram_ds; };
		Session_env const &session_env() const { return _session_env; };
};


class Net::Nic_session_root
:
	public Root_component<Nic_session_component>
{
	private:

		Env &_env;
		Quota &_shared_quota;


		/********************
		 ** Root_component **
		 ********************/

		Nic_session_component *_create_session(char const *args) override;
		void _destroy_session(Nic_session_component *session) override;

	public:

		Nic_session_root(Env &env,
		                 Allocator &alloc,
		                 Quota &shared_quota);
};


class Nic_uplink::Main
{
	private:

		Env &_env;
		Net::Quota _shared_quota { };
		Heap _heap { &_env.ram(), &_env.rm() };
		Net::Uplink_session_root _uplink_session_root { _env, _heap, _shared_quota };

	public:

		Main(Env &env);
};


/*************************************
 ** Net::Nic_session_component_base **
 *************************************/

Nic_session_component_base::Nic_session_component_base(Session_env &session_env,
                                                       size_t const tx_buf_size,
                                                       size_t const rx_buf_size)
:
	_session_env { session_env },
	_alloc { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf { _session_env, tx_buf_size },
	_rx_buf { _session_env, rx_buf_size }
{ }


/********************************
 ** Net::Nic_session_component **
 ********************************/

Net::Nic_session_component::Nic_session_component(Session_env &session_env,
                                                  size_t const tx_buf_size,
                                                  size_t const rx_buf_size,
                                                  Ram_dataspace_capability const ram_ds)
:
	Nic_session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object {
		_session_env, _tx_buf.ds(), _rx_buf.ds(), &_packet_alloc,
		_session_env.ep().rpc_ep() },
	_ram_ds { ram_ds },
	_pkt_stream_signal_handler { session_env.ep(), *this, &Nic_session_component::_handle_pkt_stream_signal }
{
	/* install packet stream signal handlers */
	_tx.sigh_packet_avail(_pkt_stream_signal_handler);
	_rx.sigh_ack_avail(_pkt_stream_signal_handler);

	/*
	 * We do not install ready_to_submit because submission is only triggered by
	 * incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */
}


void Net::Nic_session_component::_handle_pkt_stream_signal()
{
	error(__func__, __LINE__);
}


Mac_address Net::Nic_session_component::mac_address()
{
	error(__func__, __LINE__);
	return Mac_address { };
}


bool Net::Nic_session_component::link_state()
{
	error(__func__, __LINE__);
	return false;
}


void Net::Nic_session_component::link_state_sigh(Signal_context_capability sigh)
{
	_link_state_sigh = sigh;
}


/****************************************
 ** Net::Uplink_session_component_base **
 ****************************************/

Net::Uplink_session_component_base::
Uplink_session_component_base(Session_env &session_env,
                              size_t const tx_buf_size,
                              size_t const rx_buf_size)
:
	_session_env { session_env },
	_alloc { _session_env, _session_env },
	_packet_alloc { &_alloc },
	_tx_buf { _session_env, tx_buf_size },
	_rx_buf { _session_env, rx_buf_size }
{ }


/***********************************
 ** Net::Uplink_session_component **
 ***********************************/

void Net::Uplink_session_component::_handle_pkt_stream_signal()
{
	_net_if.handle_received_packets([&] (Byte_range_ptr const &src) {
		Size_guard size_guard { src.num_bytes };
		Ethernet_frame &eth { Ethernet_frame::cast_from(src.start, size_guard) };
		log("rcv ", eth);
	});
}


Net::Uplink_session_component::Uplink_session_component(Session_env &session_env,
                                                        size_t const tx_buf_size,
                                                        size_t const rx_buf_size,
                                                        Mac_address const mac,
                                                        Ram_dataspace_capability const ram_ds)
:
	Uplink_session_component_base { session_env, tx_buf_size,rx_buf_size },
	Session_rpc_object { _session_env, _tx_buf.ds(), _rx_buf.ds(),
	                                &_packet_alloc, _session_env.ep().rpc_ep() },
	_ram_ds { ram_ds },
	_pkt_stream_signal_handler { session_env.ep(), *this, &Uplink_session_component::_handle_pkt_stream_signal }
{
	/* install packet stream signal handlers */
	_tx.sigh_packet_avail(_pkt_stream_signal_handler);
	_rx.sigh_ack_avail(_pkt_stream_signal_handler);

	/*
	 * We do not install ready_to_submit because submission is only triggered
	 * by incoming packets (and dropped if the submit queue is full).
	 * The ack queue should never be full otherwise we'll be leaking packets.
	 */

	log("uplink session created! mac=", mac);
}


/******************************
 ** Net::Uplink_session_root **
 ******************************/

Net::Uplink_session_root::Uplink_session_root(Env &env,
                                              Allocator &alloc,
                                              Quota &shared_quota)
:
	Root_component<Uplink_session_component> { &env.ep().rpc_ep(), &alloc },
	_env { env },
	_shared_quota { shared_quota }
{ }


Uplink_session_component *
Net::Uplink_session_root::_create_session(char const *args)
{
	try {
		/* create session environment temporarily on the stack */
		Session_env session_env_stack { _env, _shared_quota,
			Ram_quota { Arg_string::find_arg(args, "ram_quota").ulong_value(0) },
			Cap_quota { Arg_string::find_arg(args, "cap_quota").ulong_value(0) } };

		/* alloc/attach RAM block and move session env to base of the block */
		Ram_dataspace_capability ram_ds {
			session_env_stack.alloc(sizeof(Session_env) +
			                        sizeof(Uplink_session_component), CACHED) };
		try {
			void * const ram_ptr { session_env_stack.attach(ram_ds) };
			Session_env &session_env {
				*construct_at<Session_env>(ram_ptr, session_env_stack) };

			enum { MAC_STR_LENGTH = 19 };
			char mac_str [MAC_STR_LENGTH];
			Arg mac_arg { Arg_string::find_arg(args, "mac_address") };

			if (!mac_arg.valid()) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				warning("failed to create session (failed to find 'mac_address' arg)");
				throw Service_denied();
			}
			mac_arg.string(mac_str, MAC_STR_LENGTH, "");
			Mac_address mac { };
			ascii_to(mac_str, mac);
			if (mac == Mac_address { }) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				warning("failed to create session (malformed 'mac_address' arg)");
				throw Service_denied();
			}
			/* create new session object behind session env in the RAM block */
			try {
				return construct_at<Uplink_session_component>(
					(void*)((addr_t)ram_ptr + sizeof(Session_env)),
					session_env,
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
					Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
					mac, ram_ds);
			}
			catch (Out_of_ram) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				warning("failed to create session (Uplink session RAM quota)");
				throw Insufficient_ram_quota();
			}
			catch (Out_of_caps) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				warning("failed to create session (Uplink session CAP quota)");
				throw Insufficient_cap_quota();
			}
		}
		catch (Region_map::Invalid_dataspace) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (Failed to attach RAM)");
			throw Service_denied();
		}
		catch (Region_map::Region_conflict) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (Failed to attach RAM)");
			throw Service_denied();
		}
		catch (Out_of_ram) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (Uplink session RAM quota)");
			throw Insufficient_ram_quota();
		}
		catch (Out_of_caps) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (Uplink session CAP quota)");
			throw Insufficient_cap_quota();
		}
	}
	catch (Out_of_ram) {
		warning("failed to create session (Uplink session RAM quota)");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		warning("failed to create session (Uplink session CAP quota)");
		throw Insufficient_cap_quota();
	}
}

void
Net::Uplink_session_root::_destroy_session(Uplink_session_component *session)
{
	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability ram_ds { session->ram_ds() };
	Session_env const &session_env { session->session_env() };
	session->~Uplink_session_component();

	/* copy session env to stack and detach/free all session data */
	Session_env session_env_stack { session_env };
	session_env_stack.detach(session);
	session_env_stack.detach(&session_env);
	session_env_stack.free(ram_ds);

	/* check for leaked quota */
	if (session_env_stack.ram_guard().used().value) {
		error("Uplink session component leaks RAM quota of ",
		      session_env_stack.ram_guard().used().value, " byte(s)"); };
	if (session_env_stack.cap_guard().used().value) {
		error("Uplink session component leaks CAP quota of ",
		      session_env_stack.cap_guard().used().value, " cap(s)"); };
}


/***************************
 ** Net::Nic_session_root **
 ***************************/

Net::Nic_session_root::Nic_session_root(Env &env,
                                        Allocator &alloc,
                                        Quota &shared_quota)
:
	Root_component<Nic_session_component> { &env.ep().rpc_ep(), &alloc },
	_env { env },
	_shared_quota { shared_quota }
{ }


Nic_session_component *Net::Nic_session_root::_create_session(char const *args)
{
	try {
		/* create session environment temporarily on the stack */
		Session_env session_env_stack { _env, _shared_quota,
			Ram_quota { Arg_string::find_arg(args, "ram_quota").ulong_value(0) },
			Cap_quota { Arg_string::find_arg(args, "cap_quota").ulong_value(0) } };

		/* alloc/attach RAM block and move session env to base of the block */
		Ram_dataspace_capability ram_ds {
			session_env_stack.alloc(sizeof(Session_env) +
			                        sizeof(Nic_session_component), CACHED) };
		try {
			void * const ram_ptr { session_env_stack.attach(ram_ds) };
			Session_env &session_env {
				*construct_at<Session_env>(ram_ptr, session_env_stack) };

			/* create new session object behind session env in the RAM block */
			try {
				return construct_at<Nic_session_component>(
					(void*)((addr_t)ram_ptr + sizeof(Session_env)),
					session_env,
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0),
					Arg_string::find_arg(args, "rx_buf_size").ulong_value(0),
					ram_ds);
			}
			catch (Out_of_ram) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				warning("failed to create session (NIC session RAM quota)");
				throw Insufficient_ram_quota();
			}
			catch (Out_of_caps) {
				Session_env session_env_stack { session_env };
				session_env_stack.detach(ram_ptr);
				session_env_stack.free(ram_ds);
				warning("failed to create session (NIC session CAP quota)");
				throw Insufficient_cap_quota();
			}
		}
		catch (Region_map::Invalid_dataspace) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (Failed to attach RAM)");
			throw Service_denied();
		}
		catch (Region_map::Region_conflict) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (Failed to attach RAM)");
			throw Service_denied();
		}
		catch (Out_of_ram) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (NIC session RAM quota)");
			throw Insufficient_ram_quota();
		}
		catch (Out_of_caps) {
			session_env_stack.free(ram_ds);
			warning("failed to create session (NIC session CAP quota)");
			throw Insufficient_cap_quota();
		}
	}
	catch (Out_of_ram) {
		warning("failed to create session (NIC session RAM quota)");
		throw Insufficient_ram_quota();
	}
	catch (Out_of_caps) {
		warning("failed to create session (NIC session CAP quota)");
		throw Insufficient_cap_quota();
	}
}

void Net::Nic_session_root::_destroy_session(Nic_session_component *session)
{
	/* read out initial dataspace and session env and destruct session */
	Ram_dataspace_capability ram_ds { session->ram_ds() };
	Session_env const &session_env { session->session_env() };
	session->~Nic_session_component();

	/* copy session env to stack and detach/free all session data */
	Session_env session_env_stack { session_env };
	session_env_stack.detach(session);
	session_env_stack.detach(&session_env);
	session_env_stack.free(ram_ds);

	/* check for leaked quota */
	if (session_env_stack.ram_guard().used().value) {
		error("NIC session component leaks RAM quota of ",
		      session_env_stack.ram_guard().used().value, " byte(s)"); };
	if (session_env_stack.cap_guard().used().value) {
		error("NIC session component leaks CAP quota of ",
		      session_env_stack.cap_guard().used().value, " cap(s)"); };
}


/**********************
 ** Nic_uplink::Main **
 **********************/

Nic_uplink::Main::Main(Env &env)
:
	_env { env }
{
	env.parent().announce(env.ep().manage(_uplink_session_root));
}


/***********************
 ** Genode::Component **
 ***********************/

void Component::construct(Env &env) { static Nic_uplink::Main main { env }; }
