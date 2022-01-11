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

/* os includes */
#include <net/ipv4.h>

/* lx-kit includes */
#include <lx_kit/env.h>

/* lx-emul includes */
#include <lx_emul/init.h>

/* app/wireguard includes */
#include <glue_cpp.h>
#include <base64.h>

using namespace Genode;
using namespace Net;

namespace Wireguard {

	class Main;
}


/*
 * Dummy device list implementation replacement for lx_kit
 */
Lx_kit::Device_list::Device_list(Entrypoint           &,
                                 Heap                 &,
                                 Platform::Connection &platform)
:
	_platform { platform }
{ }


class Wireguard::Main
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _config_rom      { _env, "config" };
		Attached_rom_dataspace  _private_key_rom { _env, "private_key" };

		void _handle_config()
		{
			/* update ROM contents */
			_config_rom.update();
			_private_key_rom.update();

			/* read listen port attribute */
			Xml_node const &config { _config_rom.xml() };
			glue_uint16_t listen_port {
				config.attribute_value("listen_port", (glue_uint16_t)0) };

			if (listen_port == 0) {
				class Cannot_read_listen_port { };
				throw Cannot_read_listen_port { };
			}
			/* read and decode private key from ROM */
			glue_uint8_t private_key[WG_KEY_LEN];
			{
				char private_key_base64[WG_KEY_LEN_BASE64];
				memcpy(private_key_base64,
				       _private_key_rom.local_addr<char>(),
				       WG_KEY_LEN_BASE64);

				private_key_base64[WG_KEY_LEN_BASE64 - 1] = '\0';
				if (!key_from_base64(private_key, private_key_base64)) {
					class Cannot_read_private_key { };
					throw Cannot_read_private_key { };
				}
			}
			/* install listen port and private key at contrib code */
			glue_wg_set_device_init(listen_port, private_key);
			glue_wg_open();

			/* read and apply config of each configured peer */
			config.for_each_sub_node("peer", [&] (Xml_node const &peer) {

				/* read and decode public key of the peer*/
				glue_uint8_t public_key[WG_KEY_LEN];
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
				glue_uint16_t endpoint_port { 0 };
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
							endpoint.attribute_value("port", (glue_uint16_t)0);

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
				/* install peer config at contrib code */
				glue_wg_set_device_peer(
					public_key, endpoint_ip.addr, endpoint_port);
			});
		}

	public:

		Main(Env &env) : _env(env)
		{
			Lx_kit::initialize(_env);
			lx_emul_start_kernel(nullptr);

			/* initialize wireguard device at contrib code */
			glue_wg_setup();
			glue_wg_newlink();

			_handle_config();
		}
};


void Component::construct(Env &env)
{
	static Wireguard::Main main { env };
}


extern "C" void print_hex(unsigned long x)
{
	log("--- ", Hex(x));
}
