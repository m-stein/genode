/*
 * \brief   A timer manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/cpu.h>
#include <kernel/timer.h>
#include <kernel/configuration.h>
#include <hw/assert.h>

using namespace Kernel;


/*************
 ** Timeout **
 *************/

Timeout::Timeout(Thread &thread)
:
	_type   { Type::HANDLED_BY_THREAD },
	_thread { &thread }
{ }


Timeout::Timeout()
:
	_type   { Type::NOT_HANDLED },
	_thread { nullptr }
{ }


void Timeout::handle() const
{
	switch (_type) {
	case Type::HANDLED_BY_THREAD:

		_thread->handle_timeout();
		break;

	case Type::NOT_HANDLED:

		break;
	}
}


/***************
 ** Timer_irq **
 ***************/

void Timer_irq::occurred() { _cpu.scheduler().timeout(); }


Timer_irq::Timer_irq(unsigned  id,
                     Cpu      &cpu)
:
	_irq { id, cpu.irq_pool(), cpu.pic(), *this },
	_cpu { cpu }
{ }


/***********
 ** Timer **
 ***********/

time_t Timer::timeout_max_us() const
{
	return ticks_to_us(_max_value());
}


void Timer::set_timeout(Timeout * const timeout, time_t const duration)
{
	/*
	 * Remove timeout if it is already in use. Timeouts may get overridden as
	 * result of an update.
	 */
	if (timeout->_listed)
		_timeout_list.remove(&timeout->_list_elem);
	else
		timeout->_listed = true;

	/* set timeout parameters */
	timeout->_end = time() + duration;

	/*
	 * Insert timeout. Timeouts are ordered ascending according to their end
	 * time to be able to quickly determine the nearest timeout.
	 */
	Timeout *next_shorter_timeout { nullptr };
	for (Timeout_list_element *curr_list_elem { _timeout_list.first() };
	     curr_list_elem != nullptr;
	     curr_list_elem = curr_list_elem->next())
	{
		Timeout *curr_timeout { curr_list_elem->object() };
		if (curr_timeout->_end >= timeout->_end) {
			break;
		}
		next_shorter_timeout = curr_timeout;
	}
	if (next_shorter_timeout != nullptr) {
		_timeout_list.insert(&timeout->_list_elem,
		                     &next_shorter_timeout->_list_elem);
	} else {
		_timeout_list.insert(&timeout->_list_elem);
	}
}


void Timer::schedule_timeout()
{
	/* get the timeout with the nearest end time */
	Timeout_list_element const *const list_elem { _timeout_list.first() };
	assert(list_elem);
	Timeout const *const timeout { list_elem->object() };

	/* install timeout at timer hardware */
	_time_between_schedule_calls = _duration();
	_time += _time_between_schedule_calls;
	_last_timeout_duration = (timeout->_end > _time) ? timeout->_end - _time : 1;
	_start_one_shot(_last_timeout_duration);
}


time_t Timer::time_between_schedule_calls() const
{
	return _time_between_schedule_calls;
}


void Timer::process_timeouts()
{
	/*
	 * Walk through timeouts until the first whose end time is in the future.
	 */
	time_t t = time();
	while (true) {

		Timeout_list_element const *const list_elem { _timeout_list.first() };
		if (!list_elem)
			break;

		Timeout *const timeout { list_elem->object() };

		if (timeout->_end > t)
			break;

		_timeout_list.remove(&timeout->_list_elem);
		timeout->_listed = false;
		timeout->handle();
	}
}


Timer::Timer(Cpu & cpu)
:
	_device(cpu.id()), _irq(interrupt_id(), cpu),
	_last_timeout_duration(_max_value())
{
	/*
	 * The timer frequency should allow a good accuracy on the smallest
	 * timeout syscall value (1 us).
	 */
	assert(ticks_to_us(1) < 1 || ticks_to_us(_max_value()) == _max_value());

	/*
	 * The maximum measurable timeout is also the maximum age of a timeout
	 * installed by the timeout syscall. The timeout-age syscall returns a
	 * bogus value for older timeouts. A user that awoke from waiting for a
	 * timeout might not be schedulable in the same super period anymore.
	 * However, if the user can't manage to read the timeout age during the
	 * next super period, it's a bad configuration or the users fault. That
	 * said, the maximum timeout should be at least two times the super
	 * period).
	 */
	assert(ticks_to_us(_max_value()) > 2 * cpu_quota_us);
}
