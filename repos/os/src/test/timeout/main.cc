/*
 * \brief  Test for timeout library
 * \author Martin Stein
 * \date   2016-11-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <timer_session/connection.h>
#include <os/timer.h>

using namespace Genode;


class Main
{
	private:

		using Microseconds = Genode::Timer::Microseconds;

		void _handle(Microseconds now, Cstring name) {
			log(now.value / 1000, " ms: ", name, " timeout triggered"); }

		void _handle_pt1(Microseconds now) { _handle(now, "Periodic  700ms"); }
		void _handle_pt2(Microseconds now) { _handle(now, "Periodic 1000ms"); }
		void _handle_ot1(Microseconds now) { _handle(now, "One-shot 3250ms"); }
		void _handle_ot2(Microseconds now) { _handle(now, "One-shot 5200ms"); }

		Timer::Connection      _timer_connection;
		Timer::Connection      _polling_timer_connection;
		Signal_handler<Main>   _polling_handler;
		Genode::Timer          _timer;
//		Periodic_timeout<Main> _pt1 { _timer, *this, &Main::_handle_pt1, Microseconds(700000) };
//		Periodic_timeout<Main> _pt2 { _timer, *this, &Main::_handle_pt2, Microseconds(1000000)  };
//		One_shot_timeout<Main> _ot1 { _timer, *this, &Main::_handle_ot1 };
//		One_shot_timeout<Main> _ot2 { _timer, *this, &Main::_handle_ot2 };

		enum { NR_OF_POLLS = 2000 };

		unsigned _polls { 0 };
		unsigned _polled_time_us[NR_OF_POLLS];
		unsigned _real_time_ms[NR_OF_POLLS];

		void _handle_polling()
		{
			if (_polls < NR_OF_POLLS) {
				_polled_time_us[_polls] = _timer.curr_time().value;
				if ((_polls & 0x3f) == 0) {
					_real_time_ms[_polls] = _timer_connection.elapsed_ms(); }
				else {
					_real_time_ms[_polls] = 0; }
				_polls++;
				return;
			}
			else {
				_polling_timer_connection.sigh(Signal_context_capability());
				for (unsigned i = 1; i < NR_OF_POLLS; i++) {
					char const * prefix = "";
					int diff = (int)_polled_time_us[i] - _polled_time_us[i-1];
					if (diff < 0) { prefix = "bad "; }
					else          { prefix = "good"; }

					if (_real_time_ms[i]) {
						if (_real_time_ms[i] > ((~0UL) >> 10)) {
							error("real time ms value too high"); }

						unsigned real = _real_time_ms[i] * 1000UL;
						int err       = real - _polled_time_us[i];
						log(prefix, " ", i, ": time ", _polled_time_us[i],
						                     " diff ", diff,
						                     " real ", real,
						                      " err ", err);
					} else {
						log(prefix, " ", i, ": time ", _polled_time_us[i],
						                     " diff ", diff);
					}
				}
				return;
			}
		}

	public:


		Main(Env &env) : _timer_connection(env), _polling_timer_connection(env),
		                 _polling_handler(env.ep(), *this, &Main::_handle_polling),
		                 _timer(_timer_connection, env.ep())
		{
//			_ot1.start(Microseconds(3250000));
//			_ot2.start(Microseconds(5300000));

//			for (unsigned i = 0; i < 1000 - 1; i++) {
//				if (us[i] >= us[i + 1]) {
//					error("value ", i,     ": ", us[i],
//					    ", value ", i + 1, ": ", us[i + 1]); }
//			}

			_polling_timer_connection.sigh(_polling_handler);
			_polling_timer_connection.trigger_periodic(10);
		}
};


void Component::construct(Env &env) { static Main main(env); }
