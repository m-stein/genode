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
	public:

		using Timestamp = Trace::Timestamp;

		unsigned  _local_ms        { 0 };
		Timestamp _local_ms_ts     { 0 };
		unsigned  _remote_ms       { 0 };
		Timestamp _remote_ms_ts    { 0 };
		unsigned  _ms_to_ts_factor { 0 };

enum { DMAX = 500, DMASK = 0x3, DLAY = 20};
unsigned _dms[DMAX];
unsigned _dtd[DMAX];
unsigned _dmd[DMAX];

		unsigned _curr_time_local(unsigned ts) const
		{
			unsigned local_ms_ts_diff = ts - _local_ms_ts;
			unsigned local_ms = _local_ms + (local_ms_ts_diff / _ms_to_ts_factor);

			if (local_ms < _local_ms) {
				return _local_ms + ((_local_ms - local_ms) >> 1); }
			else {
				return local_ms; }
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
unsigned old_local_ms = _local_ms;

			unsigned  remote_ms         = _session.elapsed_ms();
			Timestamp ts                = Trace::timestamp();
			unsigned  remote_ms_diff    = remote_ms - _remote_ms;
			unsigned  remote_ms_ts_diff = ts - _remote_ms_ts;
			unsigned  ms_to_ts_factor   = remote_ms_ts_diff / remote_ms_diff;

			if (remote_ms < _local_ms) {
				_local_ms = _curr_time_local(ts); }
			else {
				_local_ms = remote_ms; }

if (i > DMAX) { throw -1; }
_dtd[i] = ts - _local_ms_ts;
_dms[i] = _local_ms;
_dmd[i] = _local_ms - old_local_ms;

			_ms_to_ts_factor = (_ms_to_ts_factor + ms_to_ts_factor) >> 1;
			_remote_ms       = remote_ms;
			_remote_ms_ts    = ts;
			_local_ms_ts     = ts;

			return Microseconds(1000UL * _local_ms);
		}

		Microseconds curr_time_local(unsigned i)
		{
unsigned old_local_ms = _local_ms;

			Timestamp ts = Trace::timestamp();
			_local_ms    = _curr_time_local(ts);

if (i > DMAX) { throw -1; }
_dtd[i] = ts - _local_ms_ts;
_dms[i] = _local_ms;
_dmd[i] = _local_ms - old_local_ms;

			_local_ms_ts = ts;

			return Microseconds(1000UL * _local_ms);
		}

		void test()
		{
			using namespace Genode;
			::Timer::Connection timer;
			timer.msleep(1);
			for (unsigned i = 0; i < DMAX; i++) {
				timer.msleep(DLAY);
				if ((i & DMASK) == 0) { curr_time_remote(i).value; }
				else                  { curr_time_local(i).value; }
			}
			for (unsigned i = 1; i < DMAX; i++) {
				char const * prefix = "";
				int diff = (int)_dms[i] - _dms[i-1];

				if ((i & DMASK) == 0) { if (diff < 0) { prefix = "===== "; }
				                        else          { prefix = "----- "; } }
				else                  { if (diff < 0) { prefix = "ooooo "; }
				                        else          { prefix = "..... "; } }

//				log(dtd[i], " ", prefix, i, ": ", drms[i], " - ", dms[i], " (", diff, "/", dfac[i], ")");
				log(_dtd[i], " ", prefix, i, ": ", _dmd[i], " ", _dms[i]);
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
	{ }
};

#endif /* _TIMER_H_ */
