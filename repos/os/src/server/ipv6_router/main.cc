/*
 * \brief  Component construct and main component object
 * \author Martin Stein
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>

/* os includes */
#include <net/ipv6.h>

using namespace Genode;
using namespace Net;

namespace Ipv6_router {

	class Main;
}

class Ipv6_router::Main
{
	private:

		Env         &_env;
		Ipv6_packet  _pkt { };

	public:

		Main(Env &env);
};

Ipv6_router::Main::Main(Env &env)
:
	_env { env }
{}

void Component::construct(Env &env) { static Ipv6_router::Main main { env }; }
