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

	struct Allowed_ip : List_model<Allowed_ip>::Element
	{
		Ipv4_address_prefix ip_range;

		Allowed_ip(Ipv4_address_prefix ip_range) : ip_range(ip_range) {}
	};

	struct Allowed_ip_update_policy : List_model<Allowed_ip>::Update_policy
	{
		Genode::Allocator          & alloc;
		genode_wg_config_callbacks & callbacks;
		uint16_t                     port;
		Ipv4_address                 ip;
		uint16_t                     peer_port;

		Allowed_ip_update_policy(Allocator                  & a,
		                         genode_wg_config_callbacks & c,
		                         uint16_t                     p,
		                         Ipv4_address                 i,
		                         uint16_t                     pp)
		: alloc(a), callbacks(c), port(p), ip(i), peer_port(pp) {}

		void destroy_element(Element & e)
		{
			callbacks.remove_route(port, ip.addr, peer_port,
			                       e.ip_range.address.addr);
			destroy(alloc, &e);
		}

		Element & create_element(Xml_node node)
		{
			Ipv4_address_prefix p =
				node.attribute_value("value", Ipv4_address_prefix { });
			callbacks.add_route(port, ip.addr, peer_port,
			                    p.address.addr, p.subnet_mask().addr);
			return *(new (alloc) Element(p));
		}

		void update_element(Element &, Xml_node) {}

		static bool element_matches_xml_node(Element const & e, Xml_node node)
		{
			Ipv4_address_prefix p =
				node.attribute_value("value", Ipv4_address_prefix {});
			return (e.ip_range.address == p.address) &&
			       (e.ip_range.subnet_mask() == p.subnet_mask());
		}

		static bool node_is_element(Xml_node node) {
			return node.has_type("allowed-ip"); }
	};

	struct Peer : List_model<Peer>::Element
	{
		Key_base64             public_key;
		Ipv4_address           ip;
		uint16_t               port;
		List_model<Allowed_ip> allowed_ips {};

		Peer(Key_base64 key, Ipv4_address ip, uint16_t port)
		: public_key(key), ip(ip), port(port) { }
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
			Allowed_ip_update_policy policy(alloc, callbacks, port,
			                                e.ip, e.port);
			e.allowed_ips.destroy_all_elements(policy);
			callbacks.remove_peer(port, e.ip.addr, e.port);
			destroy(alloc, &e);
		}

		Element & create_element(Xml_node node)
		{
			uint8_t key_buf[WG_KEY_LEN];
			Ipv4_address ip = node.attribute_value("ip", Ipv4_address { });
			uint16_t      p = node.attribute_value("port", (uint16_t)0U );
			Key_base64    k = node.attribute_value("public_key", Key_base64());

			if (!k.valid() || !key_from_base64(key_buf, k.string()))
				error("Invalid public key!");

			callbacks.add_peer(port, ip.addr, p, key_buf);
			return *(new (alloc) Element(k, ip, p));
		}

		void update_element(Element & e, Xml_node node)
		{
			Allowed_ip_update_policy policy(alloc, callbacks, port,
			                                e.ip, e.port);
			e.allowed_ips.update_from_xml(policy, node);
		}

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
	Xml_node           config_node { "<invalid/>" };

	Config_model(Allocator & alloc) : alloc(alloc) {}

	void update(genode_wg_config_callbacks & callbacks)
	{
		Config_update_policy policy(alloc, callbacks);
		config.update_from_xml(policy, config_node);
	}
};


static Wireguard::Config_model & config()
{
	static Wireguard::Config_model config { Lx_kit::env().heap };
	return config;
}


class Wireguard::Main : private Entrypoint::Io_progress_handler
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config_rom      { _env, "config" };
		Signal_handler<Main>    _config_handler  { _env.ep(), *this,
		                                           &Main::_handle_config };
		Io_signal_handler<Main> _signal_handler  { _env.ep(), *this,
		                                           &Main::_handle_signal };

		void _handle_signal()
		{
			lx_user_handle_io();
			Lx_kit::env().scheduler.schedule();
		}

		void _handle_config()
		{
			_config_rom.update();
			config().config_node = _config_rom.xml();
		}

	public:

		Main(Env &env)
		:
			_env(env)
		{
			_config_rom.sigh(_config_handler);

			Lx_kit::initialize(_env);

			_handle_config();

			lx_emul_start_kernel(nullptr);

			env.ep().register_io_progress_handler(*this);
		}

		/**
		 * Entrypoint::Io_progress_handler
		 */
		void handle_io_progress() override
		{
			genode_wg_notify_peers();
		}
};


extern "C" void
genode_wg_update_config(struct genode_wg_config_callbacks * callbacks)
{
	config().update(*callbacks);
};


void Component::construct(Env &env)
{
	static Wireguard::Main main { env };
}
