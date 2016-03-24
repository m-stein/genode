/*
 * \brief   A clock manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Core includes */
#include <kernel/clock.h>

using namespace Kernel;


void Clock::_insert_timeout(Timeout * const timeout)
{
	/* timeouts may get re-inserted as result of an update */
	if (timeout->_listed) { _timeout_list.remove(timeout); }
	timeout->_listed = true;

	/* timeouts are ordered ascending according to their end time */
	Timeout * t1 = 0;
	Timeout * t2 = _timeout_list.first();
	for (; t2 && t2->_end < timeout->_end; t1 = t2, t2 = t2->next()) ;
	_timeout_list.insert(timeout, t1);
}


void Clock::_remove_timeout(Timeout * const timeout)
{
	timeout->_listed = false;
	_timeout_list.remove(timeout);
}


duration_t Clock::us_to_tics(duration_t const us) const
{
	return _timer->us_to_tics(us);
}


duration_t Clock::timeout_age_us(Timeout const * const timeout) const
{
	duration_t const age = (time_t)_time - timeout->_start;
	return _timer->tics_to_us(age);
}


duration_t Clock::timeout_max() const
{
	return _timer->tics_to_us(_timer->max_value());
}


void Clock::set_timeout(Timeout * const timeout, duration_t const duration)
{
	timeout->_start = _time;
	timeout->_end   = _time + duration;
	_insert_timeout(timeout);
}


void Clock::schedule_timeout()
{
	Timeout const * const timeout = _timeout_list.first();
	duration_t const duration = (time_t)timeout->_end - _time;
	_last_timeout_duration = duration;
	_timer->start_one_shot(duration, _cpu_id);
}


duration_t Clock::update()
{
	/* update time */
	duration_t const old_value = _last_timeout_duration;
	duration_t const new_value = _timer->value(_cpu_id);
//	PERR("%lu %lu", old_value, new_value);
	duration_t const duration  = old_value > new_value ? old_value - new_value : 1;
	_time += duration;

	/* trigger expired timeouts and remove them from the list */
	while (true) {
		Timeout * const timeout = _timeout_list.first();
		if (!timeout || timeout->_end > _time) { break; }
		_remove_timeout(timeout);
		timeout->timeout_triggered();
	}
	return duration;
}


Clock::Clock(unsigned const cpu_id, Timer * const timer)
:
	_cpu_id(cpu_id), _timer(timer) { }
