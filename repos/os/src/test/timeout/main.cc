/*
 * \brief  Test for alarm library
 * \author Norman Feske
 * \date   2008-11-05
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>

/* local includes */
#include <timer.h>

using namespace Genode;
using Microseconds = Genode::Timer::Microseconds;


class Main
{
	private:

		void _handle(Microseconds now, Cstring name) {
			log(now.value / 1000, " ms: ", name, " timeout triggered"); }

		void _handle_pt1(Microseconds now) { _handle(now, "Periodic 1s"); }
		void _handle_pt2(Microseconds now) { _handle(now, "Periodic 700ms"); }
		void _handle_ot1(Microseconds now) { _handle(now, "One-shot 3s"); }
		void _handle_ot2(Microseconds now) { _handle(now, "One-shot 5s"); }

		Timer::Connection      _timer_connection;
		Genode::Timer          _timer;
		Periodic_timeout<Main> _pt1 { _timer, *this, &Main::_handle_pt1, Microseconds(1000000) };
		Periodic_timeout<Main> _pt2 { _timer, *this, &Main::_handle_pt2, Microseconds(700000)  };
		One_shot_timeout<Main> _ot1 { _timer, *this, &Main::_handle_ot1 };
		One_shot_timeout<Main> _ot2 { _timer, *this, &Main::_handle_ot2 };

	public:

		Main(Env &env) : _timer_connection(env),
		                 _timer(_timer_connection, env.ep())
		{
			_ot1.trigger(Microseconds(3000000));
			_ot2.trigger(Microseconds(5000000));
		}
};


/***************
 ** Component **
 ***************/

size_t Component::stack_size()        { return 4 * 1024 * sizeof(addr_t); }
void   Component::construct(Env &env) { static Main main(env);            }
