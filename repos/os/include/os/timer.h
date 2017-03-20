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
	class Timer_hd_time_source;
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

		Signal_handler<Timer_time_source>  _signal_handler;
		Timeout_handler                   *_handler = nullptr;

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
#include <kernel/interface.h>

class Genode::Timer_hd_time_source : protected Timer_time_source
{
	public:

		using Timestamp = Trace::Timestamp;

		enum { FACTOR_SHIFT = 10 };

		::Timer::Session                     &_tic_session;
		Signal_handler<Timer_hd_time_source>  _tic;

		unsigned  _tic_us          { 10 };
		unsigned  _remote_us       { _tic_session.elapsed_ms() * 1000UL };
		Timestamp _remote_us_ts    { Kernel::time() };
		unsigned  _local_us        { _remote_us };
		Timestamp _local_us_ts     { _remote_us_ts };
		unsigned  _us_to_ts_factor { 1 << FACTOR_SHIFT };

unsigned _tests { 0 };
Signal_handler<Timer_hd_time_source>  _test;
::Timer::Connection _timer;
enum { DMAX = 1000, DLAY = 1000 };
unsigned _dus[DMAX];
unsigned _dtd[DMAX];
unsigned _dmd[DMAX];
unsigned _dfac[DMAX];

		unsigned _ts_to_local_us(unsigned ts) const
		{
			unsigned local_us_ts_diff = ts - _local_us_ts;
			unsigned local_us = _local_us +
			                    ((local_us_ts_diff << FACTOR_SHIFT) /
			                     _us_to_ts_factor);

			if (local_us < _local_us) {
				return _local_us + ((_local_us - local_us) >> 1); }
			else {
				return local_us; }
		}

		void _handle_tic()
		{
			unsigned volatile remote_ms = _tic_session.elapsed_ms();
			unsigned volatile ts        = Kernel::time();
			if (remote_ms > ~(unsigned)0 / 1000) {
				error("elapsed ms value too high"); }

			unsigned remote_us         = remote_ms * 1000UL;
			unsigned remote_us_diff    = remote_us - _remote_us;
			unsigned remote_us_ts_diff = ts - _remote_us_ts;
			unsigned us_to_ts_factor   = (remote_us_ts_diff << FACTOR_SHIFT) /
			                             (remote_us_diff ? remote_us_diff : 1);

			if (remote_us < _local_us) {
				_local_us = _ts_to_local_us(ts); }
			else {
				_local_us = remote_us; }

			_us_to_ts_factor = (_us_to_ts_factor + us_to_ts_factor) >> 1;
			_remote_us       = remote_us;
			_remote_us_ts    = ts;
			_local_us_ts     = ts;
		}

	public:

		Timer_hd_time_source(::Timer::Session &session,
		                     ::Timer::Session &tic_session,
		                     Entrypoint       &ep)
		: Timer_time_source(session, ep), _tic_session(tic_session),
		  _tic(ep, *this, &Timer_hd_time_source::_handle_tic)

, _test(ep, *this, &Timer_hd_time_source::_handle_test)

		{
			_tic_session.sigh(_tic);
			_tic_session.trigger_periodic(_tic_us);
		}

		Microseconds curr_time_local(unsigned i)
		{
unsigned old_local_us = _local_us;

			unsigned ts = Kernel::time();
			_local_us   = _ts_to_local_us(ts);

if (i > DMAX) { throw -1; }
_dtd[i] = ts - _local_us_ts;
_dus[i] = _local_us;
_dmd[i] = _local_us - old_local_us;
_dfac[i] = _us_to_ts_factor;

			_local_us_ts = ts;

			return Microseconds(_local_us);
		}

		void _handle_test()
		{
			using namespace Genode;
			if (_tests < DMAX) {
				curr_time_local(_tests);
				_tests++;
			} else {
				for (unsigned i = 0; i < DMAX; i++) {
					char const * prefix = "";
					int diff = (int)_dus[i] - _dus[i-1];
					if (diff < 0) { prefix = "ooo "; }
					else          { prefix = "... "; }

					log(Hex(_dtd[i], Hex::PREFIX, Hex::PAD), " ", prefix, i, ": ", _dmd[i], " ", _dus[i], " ", _dfac[i]);
				}
				while(1);
			}
		}

		void test()
		{
			using namespace Genode;
			_timer.sigh(_test);
			_timer.trigger_periodic(1000);
		}
};


/**
 * Timer-session based timeout scheduler
 *
 * Multiplexes a timer session amongst different timeouts.
 */
struct Genode::Timer : public Genode::Timer_hd_time_source,
                       public Genode::Alarm_timeout_scheduler
{
	using Time_source::Microseconds;
	using Alarm_timeout_scheduler::curr_time;

	Timer(::Timer::Session &session, ::Timer::Session &tic_session, Entrypoint &ep)
	:
		Timer_hd_time_source(session, tic_session, ep),
		Alarm_timeout_scheduler(*static_cast<Time_source*>(this))
	{ }
};

#endif /* _TIMER_H_ */
