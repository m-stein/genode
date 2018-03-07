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

		Genode::Env                            &_env;
		Interface_list                          _interfaces     { };
		Timer::Connection                       _timer          { _env };
		Genode::Heap                            _heap           { &_env.ram(), &_env.rm() };
		Genode::Attached_rom_dataspace          _config_rom     { _env, "config" };
		Genode::Reconstructible<Configuration>  _config         { _env, _config_rom.xml(), _heap, _timer };
		Signal_handler<Main>                    _config_handler { _env.ep(), *this, &Main::_handle_config };
		Uplink                                  _uplink         { _env, _timer, _heap, _interfaces, *_config };
		Net::Root                               _root           { _env.ep(), _timer, _heap, _uplink.router_mac(), *_config, _env.ram(), _interfaces, _env.rm()};

		void _handle_config();

	public:

		Main(Env &env);
};


Main::Main(Env &env) : _env(env)
{
	_config_rom.sigh(_config_handler);
	env.parent().announce(env.ep().manage(_root));
}


void Main::_handle_config()
{
	_config_rom.update();
	_config.construct(_env, _config_rom.xml(), _heap, _timer);
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
