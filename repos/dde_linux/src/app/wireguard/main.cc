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

/* lx-kit includes */
#include <lx_kit/env.h>

/* lx-emul includes */
#include <lx_emul/init.h>

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <base64.h>
#include <ipv4_address_prefix.h>

using namespace Genode;
using namespace Net;

namespace Wireguard { class Main; }


/*
 * Dummy device list implementation replacement for lx_kit
 */
Lx_kit::Device_list::Device_list(Entrypoint           &,
                                 Heap                 &,
                                 Platform::Connection &platform)
: _platform { platform } { }


class Wireguard::Main
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config_rom      { _env, "config" };
		Attached_rom_dataspace  _private_key_rom { _env, "private_key" };
		uint16_t const          _listen_port;
		char                    _private_key_base64[WG_KEY_LEN_BASE64];
		uint8_t                 _private_key[WG_KEY_LEN];

		uint16_t _read_config_listen_port();
		void     _read_config_private_key();

		void _handle_config() { _config_rom.update(); }

// FIXME: put the below XML parsing code into a List_model that can be updated!
#if 0
			/* read and apply config of each configured peer */
			config.for_each_sub_node("peer", [&] (Xml_node const &peer) {

				/* read and decode public key of the peer*/
				uint8_t public_key[WG_KEY_LEN];
				{
					String<WG_KEY_LEN_BASE64> public_key_base64 {
						peer.attribute_value(
							"public_key", String<WG_KEY_LEN_BASE64> { }) };

					if (!public_key_base64.valid()) {
						class Cannot_read_peer_public_key { };
						throw Cannot_read_peer_public_key { };
					}
					if (!key_from_base64(public_key,
					                     public_key_base64.string())) {

						class Cannot_decode_peer_public_key { };
						throw Cannot_decode_peer_public_key { };
					}
				}
				/* read endpoint config of the peer */
				Ipv4_address  endpoint_ip   { };
				uint16_t endpoint_port { 0 };
				{
					bool endpoint_missing { true };
					peer.with_sub_node(
						"endpoint", [&] (Xml_node const &endpoint)
					{
						endpoint_missing = false;

						/* read endpoint ip address */
						endpoint_ip =
							endpoint.attribute_value("ip", Ipv4_address { });

						if (endpoint_ip == Ipv4_address { }) {
							class Cannot_read_peer_endpoint_ip { };
							throw Cannot_read_peer_endpoint_ip { };
						}
						/* read endpoint port */
						endpoint_port =
							endpoint.attribute_value("port", (uint16_t)0);

						if (endpoint_port == 0) {
							class Cannot_read_peer_endpoint_port { };
							throw Cannot_read_peer_endpoint_port { };
						}
					});
					if (endpoint_missing) {
						class Cannot_read_peer_endpoint { };
						throw Cannot_read_peer_endpoint { };
					}
				}
				/* read allowed ips of the peer */
				Ipv4_address_prefix allowed_ip { };
				config.for_each_sub_node(
					"allowed-ip", [&] (Xml_node const &allowed_ip_node)
				{
					if (allowed_ip.valid()) {
						class Only_one_allowed_ip_per_peer_supported { };
						throw Only_one_allowed_ip_per_peer_supported { };
					}
					allowed_ip =
						allowed_ip_node.attribute_value(
							"value", Ipv4_address_prefix { });

					if (!allowed_ip.valid()) {
						class Cannot_read_peer_allowed_ip { };
						throw Cannot_read_peer_allowed_ip { };
					}
				});
				/* install peer config at contrib code */
				genode_wg_set_peer_config(
					public_key, endpoint_ip.addr, endpoint_port,
					allowed_ip.address.addr, allowed_ip.subnet_mask().addr);
			});
		}
#endif

	public:

		Main(Env &env);

		uint16_t listen_port() { return _listen_port; }
		uint8_t *private_key() { return _private_key; }
};


uint16_t Wireguard::Main::_read_config_listen_port()
{
	Xml_node const &config { _config_rom.xml() };
	uint16_t listen_port = config.attribute_value("listen_port",
	                                              (uint16_t)0U);

	if (listen_port == 0) {
		class Cannot_read_listen_port { };
		throw Cannot_read_listen_port { };
	}

	return listen_port;
}


void Wireguard::Main::_read_config_private_key()
{
	_private_key_rom.update();

	memcpy(_private_key_base64,
	       _private_key_rom.local_addr<char>(),
	       WG_KEY_LEN_BASE64);

	_private_key_base64[WG_KEY_LEN_BASE64 - 1] = '\0';

	if (!key_from_base64(_private_key, _private_key_base64)) {
		class Cannot_read_private_key { };
		throw Cannot_read_private_key { };
	}
}


/**
 * We need a gloabally available object here to access it from the
 * C-ish Linux kernel world
 */
static Wireguard::Main * main_object = nullptr;


Wireguard::Main::Main(Env &env)
:
	_env(env),
	_listen_port(_read_config_listen_port())
{
	main_object = this;

	_read_config_private_key();

	Lx_kit::initialize(_env);
	lx_emul_start_kernel(nullptr);
}


extern "C" void lx_user_init(void)
{
	if (!main_object)
		return;

	genode_wg_initialize_driver(main_object->listen_port(),
	                            main_object->private_key());
}


void Component::construct(Env &env)
{
	static Wireguard::Main main { env };
}


extern "C" void print_hex(unsigned long x)
{
	log("--- ", Hex(x));
}
