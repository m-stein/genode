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

namespace Genode {

	class Timer;
	class Timer_time_source;
	class Timer_time_source_interpolated;
}


/**
 * Implementation helper for 'Timer'
 *
 * \noapi
 */
class Genode::Timer_time_source : public Genode::Time_source
{
	protected:

		::Timer::Session &_session;

	private:

		enum { MIN_TIMEOUT_US = 5000 };

		using Signal_handler = Genode::Signal_handler<Timer_time_source>;

		Signal_handler    _signal_handler;
		Timeout_handler  *_handler = nullptr;

		void _handle_timeout()
		{
			if (_handler)
				_handler->handle_timeout(curr_time());
		}

	public:

		Timer_time_source(::Timer::Session &session, Entrypoint &ep)
		:
			_session(session),
			_signal_handler(ep, *this, &Timer_time_source::_handle_timeout)
		{
			_session.sigh(_signal_handler);
		}

		virtual Microseconds curr_time() override {
			return Microseconds(1000UL * _session.elapsed_ms()); }

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
};

#include <trace/timestamp.h>

class Genode::Timer_time_source_interpolated : protected Timer_time_source
{
	private:

		Microseconds real_time       { 0 };
		Trace::Timestamp time_stamp  { 0 };
		Microseconds interpol_time   { 0 };
		Microseconds interpol_factor { 0 };

	public:

		Timer_time_source_interpolated(::Timer::Session &session,
		                               Entrypoint       &ep)
		: Timer_time_source(session, ep) { }

		Microseconds curr_time() override
		{

	enum { QUOTA_LIMIT_LOG2 = 15 };
	enum { QUOTA_LIMIT = 1 << QUOTA_LIMIT_LOG2 };

			using namespace Trace;
			Microseconds new_real_time = Microseconds(1000UL * _session.elapsed_ms());
			Timestamp new_time_stamp   = timestamp();
			unsigned real_time_diff    = new_real_time.value - real_time.value;
			unsigned time_stamp_diff   = new_time_stamp - time_stamp;
			Trace Timestamp =  * time_stamp_diff / real_time_diff
			log("ts diff ", time_stamp_diff, " us diff ", real_time_diff, " factor ", );
			time_stamp = new_time_stamp;
			real_time  = new_real_time;
			return real_time;
		}
};


/**
 * Timer-session based timeout scheduler
 *
 * Multiplexes a timer session amongst different timeouts.
 */
struct Genode::Timer : private Genode::Timer_time_source_interpolated,
                       public  Genode::Alarm_timeout_scheduler
{
	using Time_source::Microseconds;
	using Alarm_timeout_scheduler::curr_time;

	Timer(::Timer::Session &session, Entrypoint &ep)
	:
		Timer_time_source_interpolated(session, ep),
		Alarm_timeout_scheduler(*(Time_source*)this)
	{ }
};

#endif /* _TIMER_H_ */
