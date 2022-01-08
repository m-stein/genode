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
#include <glue_cpp.h>

using namespace Genode;

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
		Attached_rom_dataspace  _config_rom { _env, "config" };

		void _handle_config()
		{
			_config_rom.update();
			Xml_node const &node { _config_rom.xml() };
			glue_uint16_t listen_port {
				node.attribute_value("listen_port", (uint16_t)0) };

			if (listen_port == 0) {
				class Failed_to_get_listen_port { };
				throw Failed_to_get_listen_port { };
			}
			glue_wg_set(listen_port);
		}

	public:

		Main(Env &env) : _env(env)
		{
			Lx_kit::initialize(_env);
			lx_emul_start_kernel(nullptr);
			_handle_config();
		}
};


void Component::construct(Env &env)
{
	static Wireguard::Main main { env };
}
