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
#include <kernel/interface.h>

class Genode::Timer_time_source_interpolated : protected Timer_time_source
{
	public:

		using Timestamp = Trace::Timestamp;

		unsigned  _local_us        { 0 };
		Timestamp _local_us_ts     { 0 };
		unsigned  _remote_us       { 0 };
		Timestamp _remote_us_ts    { 0 };
		unsigned  _us_to_ts_factor { 0 };

enum { DMAX = 2000, DMASK = 0x1ff, DLAY = 1000 };
unsigned _dus[DMAX];
unsigned _dtd[DMAX];
unsigned _dmd[DMAX];
unsigned _dfac[DMAX];

		unsigned _curr_time_local(unsigned ts) const
		{
			unsigned local_us_ts_diff = ts - _local_us_ts;
			unsigned local_us = _local_us + ((local_us_ts_diff << 10) / _us_to_ts_factor);

			if (local_us < _local_us) {
				return _local_us + ((_local_us - local_us) >> 1); }
			else {
				return local_us; }
		}

	public:

		Timer_time_source_interpolated(::Timer::Session &session,
		                               Entrypoint       &ep)
		: Timer_time_source(session, ep) { }

		Microseconds curr_time() override
		{
			return Microseconds(1);
		}


		Microseconds curr_time_remote(unsigned i)
		{
unsigned old_local_us = _local_us;

			unsigned volatile remote_ms = _session.elapsed_ms();
			unsigned volatile ts        = Kernel::time();
			if (remote_ms > ~(unsigned)0 / 1000) {
				error("elapsed ms value too high"); }

			unsigned remote_us         = remote_ms * 1000UL;
			unsigned remote_us_diff    = remote_us - _remote_us;
			unsigned remote_us_ts_diff = ts - _remote_us_ts;
			unsigned us_to_ts_factor   = (remote_us_ts_diff << 10) /
			                             (remote_us_diff ? remote_us_diff : 1);

			if (remote_us < _local_us) {
				_local_us = _curr_time_local(ts); }
			else {
				_local_us = remote_us; }

if (i > DMAX) { throw -1; }
_dtd[i] = ts - _local_us_ts;
_dus[i] = _local_us;
_dmd[i] = _local_us - old_local_us;
_dfac[i] = _us_to_ts_factor;

			_us_to_ts_factor = (_us_to_ts_factor + us_to_ts_factor) >> 1;
			_remote_us       = remote_us;
			_remote_us_ts    = ts;
			_local_us_ts     = ts;

			return Microseconds(_local_us);
		}

		Microseconds curr_time_local(unsigned i)
		{
unsigned old_local_us = _local_us;

			unsigned ts = Kernel::time();
			_local_us   = _curr_time_local(ts);

if (i > DMAX) { throw -1; }
_dtd[i] = ts - _local_us_ts;
_dus[i] = _local_us;
_dmd[i] = _local_us - old_local_us;
_dfac[i] = _us_to_ts_factor;

			_local_us_ts = ts;

			return Microseconds(_local_us);
		}

		void test()
		{
			using namespace Genode;
			::Timer::Connection timer;
//			timer.usleep(1);

//			unsigned y = 0;
//			for (unsigned i = 0; ; i++) {
//				unsigned x = Kernel::time();
//				Genode::raw(x - y);
//				y = x;
//				timer.usleep(1000000);
//			}

			for (unsigned i = 0; i < DMAX; i++) {
				timer.usleep(1000);
//				for (unsigned volatile i = 0; i < DLAY; i++) { }
				if ((i & DMASK) == 0) { curr_time_remote(i).value; }
				else                  { curr_time_local(i).value; }
			}
			for (unsigned i = 1; i < DMAX; i++) {
				char const * prefix = "";
				int diff = (int)_dus[i] - _dus[i-1];

				if ((i & DMASK) == 0) { if (diff < 0) { prefix = "===== "; }
				                        else          { prefix = "----- "; } }
				else                  { if (diff < 0) { prefix = "ooooo "; }
				                        else          { prefix = "..... "; } }

//				log(dtd[i], " ", prefix, i, ": ", drus[i], " - ", dus[i], " (", diff, "/", dfac[i], ")");
				log(Hex(_dtd[i], Hex::PREFIX, Hex::PAD), " ", prefix, i, ": ", _dmd[i], " ", _dus[i], " ", _dfac[i]);
			}
		}
};


/**
 * Timer-session based timeout scheduler
 *
 * Multiplexes a timer session amongst different timeouts.
 */
struct Genode::Timer : public Genode::Timer_time_source_interpolated,
                       public Genode::Alarm_timeout_scheduler
{
	using Time_source::Microseconds;
	using Alarm_timeout_scheduler::curr_time;

	Timer(::Timer::Session &session, Entrypoint &ep)
	:
		Timer_time_source_interpolated(session, ep),
		Alarm_timeout_scheduler(*(Time_source*)this)
	{
	}
};

#endif /* _TIMER_H_ */
