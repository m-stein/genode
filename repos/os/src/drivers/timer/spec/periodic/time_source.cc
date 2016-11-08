/*
 * \brief  Time source based on spinning usleep
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <time_source.h>

using namespace Genode;


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	Genode::Lock::Guard lock_guard(_lock);
	Threaded_time_source::handler(handler);
	_next_timeout = duration;
}


Timer::Time_source::Time_source(Entrypoint &ep)
:
	Threaded_time_source(ep), _next_timeout(max_timeout())
{
	Thread::start();
}


void Timer::Time_source::_wait_for_irq()
{
	enum { SLEEP_GRANULARITY_US = 1000UL };
	Microseconds last_time = curr_time();
	_lock.lock();
	while (_next_timeout > 0) {
		_lock.unlock();

		try { _usleep(SLEEP_GRANULARITY_US); }
		catch (Blocking_canceled) { }

		Microseconds now_time       = curr_time();
		Microseconds sleep_duration = now_time - last_time;
		last_time = now_time;

		_lock.lock();
		if (_next_timeout >= sleep_duration)
			_next_timeout -= sleep_duration;
		else
			break;
	}
	_lock.unlock();
}
