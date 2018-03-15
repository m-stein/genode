/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <nic/xml_node.h>
#include <timer_session/connection.h>

/* local includes */
#include <component.h>
#include <uplink.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


class Main
{
	private:

		Genode::Env                    &_env;
		Interface_list                  _interfaces     { };
		Timer::Connection               _timer          { _env };
		Genode::Heap                    _heap           { &_env.ram(), &_env.rm() };
		Genode::Attached_rom_dataspace  _config_rom     { _env, "config" };
		Pointer<Configuration>          _config         { _init_config() };
		Signal_handler<Main>            _config_handler { _env.ep(), *this, &Main::_handle_config };
		Uplink                          _uplink         { _env, _timer, _heap, _interfaces, _config.deref() };
		Net::Root                       _root           { _env.ep(), _timer, _heap, _uplink.router_mac(), _config.deref(), _env.ram(), _interfaces, _env.rm()};

		void _handle_config();

		Configuration &_init_config();

	public:

		Main(Env &env);
};


Configuration &Main::_init_config()
{
	Configuration &config_legacy = *new (_heap)
		Configuration(_config_rom.xml(), _heap);

	Configuration &config = *new (_heap)
		Configuration(_env, _config_rom.xml(), _heap, _timer, config_legacy);

	destroy(_heap, &config_legacy);
	return config;
}


Main::Main(Env &env) : _env(env)
{
	_config_rom.sigh(_config_handler);
	env.parent().announce(env.ep().manage(_root));
}


void Main::_handle_config()
{
	_config_rom.update();
	Configuration &old_config = _config.deref();
	Configuration &new_config = *new (_heap)
		Configuration(_env, _config_rom.xml(), _heap, _timer, old_config);

	_root.handle_config(new_config);
	_interfaces.for_each([&] (Net::Interface &interface) {
		interface.handle_config(new_config); });

	destroy(_heap, &old_config);
	_config = Pointer<Configuration>(new_config);
}


void Component::construct(Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	try { static Main main(env); }

	catch (Net::Domain_tree::No_match) {
		error("failed to find configuration for domain 'uplink'");
		env.parent().exit(-1);
	}
}
