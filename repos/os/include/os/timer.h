/*
 * \brief  Multiplexes a timer session amongst different timeouts
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* Genode includes */
#include <timer_session/timer_session.h>
#include <os/time_source.h>
#include <os/timeout.h>
#include <trace/timestamp.h>
#include <kernel/interface.h>

namespace Genode {

	class Timer;
	class Timer_time_source;
}


/**
 * Implementation helper for 'Timer'
 *
 * \noapi
 */
class Genode::Timer_time_source : public Genode::Time_source
{
	private:

		using Timestamp = Trace::Timestamp;

		enum { FACTOR_SHIFT   = 10 };
		enum { MIN_TIMEOUT_US = 5000 };
		enum { TIC_US         = 100000 };

		::Timer::Session                                   &_session;
		Signal_handler<Timer_time_source>                   _signal_handler;
		Timeout_handler                                    *_handler = nullptr;
		Constructible<Periodic_timeout<Timer_time_source> > _tic;

		unsigned     _us              { _session.elapsed_ms() * 1000UL };
		unsigned     _interpolated_us { 0 };
		Timestamp    _ts              { Kernel::time() };
		unsigned     _us_to_ts_factor { 1 << FACTOR_SHIFT };

		void _handle_tic(Microseconds)
		{
			unsigned volatile ms = _session.elapsed_ms();
			unsigned volatile ts = Kernel::time();

			if (ms > ((~0UL) >> 10)) {
				error("elapsed ms value too high"); }

			unsigned us      = ms * 1000UL;
			unsigned us_diff = us - _us;
			unsigned ts_diff = ts - _ts;
			unsigned us_to_ts_factor = (ts_diff << FACTOR_SHIFT) /
			                           (us_diff ? us_diff : 1);

			_us = us;
			_ts = ts;
			_us_to_ts_factor = us_to_ts_factor;
		}

		void _handle_timeout()
		{
			if (_handler)
				_handler->handle_timeout(curr_time());
		}

	public:

		Timer_time_source(::Timer::Session &session,
		                  Entrypoint       &ep)
		:
			_session(session),
			_signal_handler(ep, *this, &Timer_time_source::_handle_timeout)
		{
			_session.sigh(_signal_handler);
		}

		void schedule_timeout(Microseconds     duration,
		                      Timeout_handler &handler) override
		{
			if (duration.value < MIN_TIMEOUT_US)
				duration.value = MIN_TIMEOUT_US;

			if (duration.value > max_timeout().value)
				duration.value = max_timeout().value;

			_handler = &handler;
			_session.trigger_once(duration.value);
		}

		Microseconds max_timeout() const override { return Microseconds::max(); }

		Microseconds curr_time() override
		{
			unsigned ts_diff = Kernel::time() - _ts;
			unsigned us_diff = (ts_diff << FACTOR_SHIFT) / _us_to_ts_factor;
			unsigned us      = _us + us_diff;
			if (_interpolated_us < us) {
				_interpolated_us = us; }
			return Microseconds(_interpolated_us);
		}

		void schedule_tic(Timeout_scheduler &scheduler) override
		{
			if (_tic.constructed()) {
				return; }

			_tic.construct(scheduler, *this, &Timer_time_source::_handle_tic,
			               Microseconds(TIC_US));
		}
};


/**
 * Timer-session based timeout scheduler
 *
 * Multiplexes a timer session amongst different timeouts.
 */
struct Genode::Timer : public Genode::Timer_time_source,
                       public Genode::Alarm_timeout_scheduler
{
	using Time_source::Microseconds;
	using Alarm_timeout_scheduler::curr_time;

	Timer(::Timer::Session &session, Entrypoint &ep)
	:
		Timer_time_source(session, ep),
		Alarm_timeout_scheduler(*static_cast<Time_source*>(this))
	{ }
};

#endif /* _TIMER_H_ */
