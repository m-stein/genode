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
#include <trace/timestamp.h>

using namespace Genode;

void Component::construct(Genode::Env &env)
{
	Timer::Connection timer(env);
	unsigned long id = 0;
	while (1) {
		timer.msleep(1000);
		Thread::trace(String<32>(id, " ", Trace::timestamp()).string());
		id++;
	}
}
