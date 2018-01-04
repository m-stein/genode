/*
 * \brief  Log information about present trace subjects
 * \author Norman Feske
 * \date   2015-06-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>

using namespace Genode;

void Component::construct(Genode::Env &env)
{
	Timer::Connection timer(env);
	while (1) {
		timer.msleep(1000);
		log("trace event");
		Thread::trace("XXX");
	}
}
