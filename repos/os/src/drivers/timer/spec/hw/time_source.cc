/*
 * \brief  Platform-specific timer back end
 * \author Martin Stein
 * \date   2012-05-03
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/signal.h>
#include <base/entrypoint.h>

/* local includes */
#include <time_source.h>

/* base-hw includes */
#include <kernel/interface.h>

using namespace Genode;
using namespace Timer;
using Microseconds = Genode::Time_source::Microseconds;

enum { MIN_TIMEOUT_US = 1000 };


Timer::Time_source::Time_source(Entrypoint &ep)
:
	Signalled_time_source(ep),
	_max_timeout(Kernel::timeout_max_us())
{
	if (_max_timeout < MIN_TIMEOUT_US) {
		error("minimum timeout greater then maximum timeout");
		throw Genode::Exception();
	}
}


void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	if (duration < MIN_TIMEOUT_US) { duration = MIN_TIMEOUT_US; }
	if (duration > max_timeout())  { duration = max_timeout(); }
	_handler = &handler;
	_last_timeout_age = 0;
	Kernel::timeout(duration, (addr_t)_signal_handler.data());
}


Microseconds Timer::Time_source::curr_time() const
{
	Microseconds const timeout_age = Kernel::timeout_age_us();
	if (timeout_age > _last_timeout_age) {

		/* increment time by the difference since the last update */
		_curr_time += timeout_age - _last_timeout_age;
		_last_timeout_age = timeout_age;
	}
	return _curr_time;
}
