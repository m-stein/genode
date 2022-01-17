/*
 * \brief  Wireguard component
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <util/list_model.h>

#include <base/session_label.h>
#include <nic_session/connection.h>
#include <uplink_session/connection.h>
#include <nic/packet_allocator.h>

/* lx-kit includes */
#include <lx_kit/env.h>

/* lx-emul includes */
#include <lx_emul/init.h>

/* lx-user includes */
#include <lx_user/io.h>

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <base64.h>
#include <ipv4_address_prefix.h>

using namespace Genode;
using namespace Net;

namespace Wireguard {
	struct Config_model;
	class  Main;
}


/*
 * Dummy device list implementation replacement for lx_kit
 */
Lx_kit::Device_list::Device_list(Entrypoint           &,
                                 Heap                 &,
                                 Platform::Connection &platform)
: _platform { platform } { }


struct Wireguard::Config_model
{
	using Key_base64 = String<WG_KEY_LEN_BASE64>;

	struct Peer : List_model<Peer>::Element
	{
		Key_base64             public_key;
		Ipv4_address           ip;
		uint16_t               port;
		Ipv4_address_prefix    allowed_ip;

		Peer(Key_base64 key, Ipv4_address ip, uint16_t port, Ipv4_address_prefix allowed_ip)
		: public_key(key), ip(ip), port(port), allowed_ip(allowed_ip) { }
	};

	struct Peer_update_policy : List_model<Peer>::Update_policy
	{
		Genode::Allocator          & alloc;
		genode_wg_config_callbacks & callbacks;
		uint16_t                     port;

		Peer_update_policy(Allocator                  & a,
		                   genode_wg_config_callbacks & c,
		                   uint16_t                     p)
		: alloc(a), callbacks(c), port(p) {}

		void destroy_element(Element & e)
		{
			callbacks.remove_peer(port, e.ip.addr, e.port);
			destroy(alloc, &e);
		}

		Element & create_element(Xml_node node)
		{
			uint8_t key_buf[WG_KEY_LEN];
			Ipv4_address        ip = node.attribute_value("ip", Ipv4_address { });
			uint16_t             p = node.attribute_value("port", (uint16_t)0U );
			Key_base64           k = node.attribute_value("public_key", Key_base64());
			Ipv4_address_prefix  a = node.attribute_value("allowed_ip", Ipv4_address_prefix());

			if (!k.valid() || !key_from_base64(key_buf, k.string()))
				error("Invalid public key!");

			if (!a.valid())
				error("Invalid allowed ip!");

			callbacks.add_peer(
				port, ip.addr, p, key_buf, a.address.addr, a.prefix);

			return *(new (alloc) Element(k, ip, p, a));
		}

		void update_element(Element &, Xml_node) { }

		static bool element_matches_xml_node(Element const & e, Xml_node node)
		{
			Ipv4_address ip = node.attribute_value("ip", Ipv4_address { });
			uint16_t      p = node.attribute_value("port", (uint16_t)0U );
			Key_base64    k = node.attribute_value("public_key", Key_base64());

			return (ip == e.ip) && (p == e.port) && (k == e.public_key);
		}

		static bool node_is_element(Xml_node node) {
			return node.has_type("peer"); }
	};

	struct Config : List_model<Config>::Element
	{
		Key_base64       private_key;
		uint16_t         port;
		List_model<Peer> peers {};

		Config(Key_base64 key, uint16_t port) : private_key(key), port(port) {}
	};

	struct Config_update_policy : List_model<Config>::Update_policy
	{
		Genode::Allocator          & alloc;
		genode_wg_config_callbacks & callbacks;

		Config_update_policy(Allocator & a, genode_wg_config_callbacks & c)
		: alloc(a), callbacks(c) {}

		void destroy_element(Element & e)
		{
			Peer_update_policy policy(alloc, callbacks, e.port);
			e.peers.destroy_all_elements(policy);
			callbacks.remove_device(e.port);
			destroy(alloc, &e);
		}

		Element & create_element(Xml_node node)
		{
			uint8_t    key_buf[WG_KEY_LEN];
			Key_base64 key  = node.attribute_value("private_key", Key_base64());
			uint16_t   port = node.attribute_value("listen_port", (uint16_t)0U);

			if (!key.valid() || !key_from_base64(key_buf, key.string()))
				error("Invalid private key!");

			callbacks.add_device(port, key_buf);
			return *(new (alloc) Element(key, port));
		}

		void update_element(Element & e, Xml_node node)
		{
			Peer_update_policy policy(alloc, callbacks, e.port);
			e.peers.update_from_xml(policy, node);
		}

		static bool element_matches_xml_node(Element const & e, Xml_node node)
		{
			return e.port == node.attribute_value("listen_port", (uint16_t)0U);
		}

		static bool node_is_element(Xml_node node) {
			return node.has_type("iface"); }
	};

	Allocator        & alloc;
	List_model<Config> config {};

	Config_model(Allocator & alloc) : alloc(alloc) {}

	void update(genode_wg_config_callbacks & callbacks, Xml_node node)
	{
		Config_update_policy policy(alloc, callbacks);
		config.update_from_xml(policy, node);
	}
};


class Wireguard::Main : private Entrypoint::Io_progress_handler
{
	private:

		Env                    &_env;
		Heap                    _heap            { _env.ram(), _env.rm() };
		Attached_rom_dataspace  _config_rom      { _env, "config"        };
		Signal_handler<Main>    _config_handler  { _env.ep(), *this,
		                                           &Main::_handle_config };
		Io_signal_handler<Main> _signal_handler  { _env.ep(), *this,
		                                           &Main::_handle_signal };
		Config_model            _config_model    { _heap                 };

		enum { PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE  };
		enum { BUF_SIZE    = Uplink::Session::QUEUE_SIZE * PACKET_SIZE   };

		Nic::Packet_allocator   _packet_alloc_up { &_heap                };
		Nic::Packet_allocator   _packet_alloc_dw { &_heap                };
		Net::Mac_address const  _mac_address     { 3U                    };
		Uplink::Connection      _uplink          { _env, &_packet_alloc_up,
		                                           BUF_SIZE, BUF_SIZE,
		                                           _mac_address, "uplink"};
		Nic::Connection         _downlink        { _env, &_packet_alloc_dw,
		                                           BUF_SIZE, BUF_SIZE,
		                                           "down"};
		bool                    _notify_peers    { true };

		void _handle_signal()
		{
			lx_user_handle_io();
			Lx_kit::env().scheduler.schedule();
		}

		void _handle_config() { _config_rom.update(); }

		template <typename SINK>
		void _for_each_rx_packet(SINK & rx_sink, genode_wg_net_receive_t func)
		{
			for (;;) {

				if (!rx_sink.packet_avail() || !rx_sink.ack_slots_free())
					break;

				typedef Uplink::Packet_descriptor Packet_descriptor;

				Packet_descriptor const packet = rx_sink.peek_packet();

				bool const packet_valid = rx_sink.packet_valid(packet)
				                       && (packet.offset() >= 0);

				void * content = rx_sink.packet_content(packet);

				//FIXME: get listen port and put it into callback
				if (packet_valid) func(0U, content, packet.size());

				_notify_peers = true;

				(void)rx_sink.try_get_packet();
				rx_sink.try_ack_packet(packet);
			}
		}

		template <typename SOURCE>
		bool _tx_one_packet(SOURCE & tx_source, void * buf, size_t buf_size)
		{
			/*
			 * Process acknowledgements
			 */

			while (tx_source.ack_avail()) {
				tx_source.release_packet(tx_source.try_get_acked_packet());
				_notify_peers = true;
			}

			/*
			 * Submit packet
			 */

			if (!tx_source.ready_to_submit(1))
				return false;

			typedef Uplink::Packet_descriptor Packet_descriptor;

			Packet_descriptor packet { };
			size_t const max_bytes = Nic::Packet_allocator::OFFSET_PACKET_SIZE;

			try { packet = tx_source.alloc_packet(max_bytes); }
			catch (Uplink::Session::Tx::Source::Packet_alloc_failed) {
				return false; /* packet-stream buffer is saturated */ }

			char * const dst_ptr = tx_source.packet_content(packet);
			size_t const payload_bytes = min(max_bytes, buf_size);
			memcpy(dst_ptr, buf, payload_bytes);

			/* imprint payload size into packet descriptor */
			packet = Packet_descriptor(packet.offset(), payload_bytes);

			tx_source.try_submit_packet(packet);
			_notify_peers = true;

			return true;
		}

	public:

		Main(Env &env)
		:
			_env(env)
		{
			Lx_kit::initialize(_env);

			_config_rom.sigh(_config_handler);
			_handle_config();

			env.ep().register_io_progress_handler(*this);

			_uplink.rx_channel()->sigh_ready_to_ack   (_signal_handler);
			_uplink.rx_channel()->sigh_packet_avail   (_signal_handler);
			_uplink.tx_channel()->sigh_ack_avail      (_signal_handler);
			_uplink.tx_channel()->sigh_ready_to_submit(_signal_handler);

			_downlink.rx_channel()->sigh_ready_to_ack   (_signal_handler);
			_downlink.rx_channel()->sigh_packet_avail   (_signal_handler);
			_downlink.tx_channel()->sigh_ack_avail      (_signal_handler);
			_downlink.tx_channel()->sigh_ready_to_submit(_signal_handler);

			/* trigger signal handling once after construction */
			Signal_transmitter(_signal_handler).submit();
		}

		/**
		 * Entrypoint::Io_progress_handler
		 */
		void handle_io_progress() override
		{
			if (_notify_peers) {
				_notify_peers = false;
				_uplink.rx()->wakeup();
				_uplink.tx()->wakeup();
				_downlink.rx()->wakeup();
				_downlink.tx()->wakeup();
			}
		}

		void update(genode_wg_config_callbacks & callbacks)
		{
			_config_model.update(callbacks, _config_rom.xml());
		}

		void net_receive(genode_wg_net_receive_t rcv_callback)
		{
			_for_each_rx_packet(*_uplink.rx(), rcv_callback);
			_for_each_rx_packet(*_downlink.rx(), rcv_callback);
		}

		bool net_send(void * buf, size_t buf_size, bool up)
		{
			return up ? _tx_one_packet(*_uplink.tx(), buf, buf_size)
			          : _tx_one_packet(*_downlink.tx(), buf, buf_size);
		}
};


static Wireguard::Main & main_object(Genode::Env & env)
{
	static Wireguard::Main main { env };
	return main;
}


extern "C" void
genode_wg_update_config(struct genode_wg_config_callbacks * callbacks)
{
	main_object(Lx_kit::env().env).update(*callbacks);
};


extern "C" void
genode_wg_net_receive(genode_wg_net_receive_t rcv_callback)
{
	main_object(Lx_kit::env().env).net_receive(rcv_callback);
}


extern "C" int
genode_wg_net_send(void * buf, unsigned long buf_size, int up)
{
	return (main_object(Lx_kit::env().env).net_send(buf, buf_size, up))
		? 0 : -1;
}


void Component::construct(Env &env)
{
	main_object(env);

	/*
	 * Main needs to be constructed before startin Linux code,
	 * because of genode_wg_* calls
	 */
	lx_emul_start_kernel(nullptr);
}
