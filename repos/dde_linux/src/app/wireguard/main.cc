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

/* lx-kit includes */
#include <lx_kit/env.h>
#include <lx_emul/init.h>

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

		Env &_env;


	public:

		Main(Env &env) : _env(env)
	{
		Lx_kit::initialize(_env);
		lx_emul_start_kernel(nullptr);
	}
};


void Component::construct(Env &env)
{
	static Wireguard::Main main { env };
}
