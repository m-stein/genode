/*
 * \brief  Wireguard component
 * \author Stefan Kalkowski
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <lx_kit/env.h>
#include <lx_emul/init.h>

using namespace Genode;

/** Dummy device list implementation replacement for lx_kit **/
Lx_kit::Device_list::Device_list(Entrypoint&, Heap&, Platform::Connection & p)
: _platform(p) {}


struct Main
{
	Env & env;

	Main(Env & env) : env(env)
	{
		Lx_kit::initialize(env);
		lx_emul_start_kernel(nullptr);
	}
};


void Component::construct(Env & env)
{
	static Main main(env);
}
