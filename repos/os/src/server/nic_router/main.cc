/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/component.h>
#include <base/heap.h>
#include <os/config.h>
#include <nic/xml_node.h>

/* local includes */
#include <component.h>
#include <uplink.h>
#include <configuration.h>

using namespace Net;
using namespace Genode;


class Main
{
	private:

		Genode::Heap  _heap;
		Configuration _config;
		Uplink        _uplink;
		Net::Root     _root;

	public:

		Main(Env &env);
};


Main::Main(Env &env)
:
	_heap(&env.ram(), &env.rm()), _config(config()->xml_node(), _heap),
	_uplink(env.ep(), _heap, _config),
	_root(env.ep(), _heap, _uplink.router_mac(), _config, env.ram())
{
	env.parent().announce(env.ep().manage(_root));
}


/***************
 ** Component **
 ***************/

size_t Component::stack_size()        { return 4 * 1024 * sizeof(addr_t); }
void   Component::construct(Env &env) { static Main main(env); }
