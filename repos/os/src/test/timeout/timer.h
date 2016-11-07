/*
 * \brief  Multiplexes a timer connection amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <os/time_source.h>
#include <os/timeout.h>

namespace Genode { class Timer; }


class Genode::Timer : public Timeout_scheduler
{
	private:

		class Time_source : public Genode::Time_source
		{
			private:

				enum { MIN_TIMEOUT_US = 100000 };

				using Signal_handler =
					Genode::Signal_handler<Time_source>;

				::Timer::Connection  _connection;
				Signal_handler       _signal_handler;
				Timeout_handler     *_handler = nullptr;

				void _handle_timeout()
				{
					if (_handler) {
						_handler->handle_timeout(curr_time()); }
				}

			public:

				Time_source(Env &env, Entrypoint &ep)
				:
					_connection(env),
					_signal_handler(ep, *this, &Time_source::_handle_timeout)
				{
					_connection.sigh(_signal_handler);
				}


				/*************************
				 ** Genode::Time_source **
				 *************************/

				Microseconds curr_time() const override
				{
					return (unsigned long long)_connection.elapsed_ms() * 1000;
				}

				void schedule_timeout(Microseconds     duration,
				                      Timeout_handler &handler) override
				{
					if (duration < MIN_TIMEOUT_US) {
						duration = MIN_TIMEOUT_US; }

					if (duration > max_timeout()) {
						duration = max_timeout(); }

					_handler = &handler;
					_connection.trigger_once(duration);
				}

				Microseconds max_timeout() const override { return ~0UL; }

		} _time_source;

		Alarm_timeout_scheduler _timeout_scheduler { _time_source };

	public:

		Timer(Env &env, Entrypoint &ep) : _time_source(env, ep) { }


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		void schedule_periodic(Timeout            &timeout,
		                       Microseconds const  duration) override
		{
			_timeout_scheduler.schedule_periodic(timeout, duration);
		}

		void schedule_one_shot(Timeout            &timeout,
		                       Microseconds const  duration) override
		{
			_timeout_scheduler.schedule_one_shot(timeout, duration);
		}

		Microseconds curr_time() const override
		{
			return _timeout_scheduler.curr_time();
		}

		void discard(Timeout &timeout) override
		{
			_timeout_scheduler.discard(timeout);
		}
};

#endif /* _TIMER_H_ */
