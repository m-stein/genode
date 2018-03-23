/*
 * \brief  Multiplexing one time source amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer/timeout.h>

using namespace Genode;


/*************
 ** Timeout **
 *************/

void Timeout::schedule_periodic(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = true;
	_alarm.timeout_scheduler._schedule_periodic(*this, duration);
}


void Timeout::schedule_one_shot(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = false;
	_alarm.timeout_scheduler._schedule_one_shot(*this, duration);
}


void Timeout::discard()
{
	_alarm.timeout_scheduler._discard(*this);
	_alarm.handler = nullptr;
}


/********************
 ** Timeout::Alarm **
 ********************/

bool Timeout::Alarm::_on_alarm(unsigned)
{
	if (handler) {
		Handler *current = handler;
		if (!periodic) {
			handler = nullptr;
		}
		current->handle_timeout(timeout_scheduler.curr_time());
	}
	return periodic;
}


Timeout::Alarm::~Alarm()
{
	if (_scheduler)
		_scheduler->_alarm__discard(this);
}


bool Timeout::Alarm::Raw::is_pending_at(unsigned long time, bool time_period) const
{
	return (time_period == deadline_period &&
	        time        >= deadline) ||
	       (time_period != deadline_period &&
	        time        <  deadline);
}


/*****************************
 ** Alarm_timeout_scheduler **
 *****************************/

void Alarm_timeout_scheduler::handle_timeout(Duration duration)
{
	unsigned long const curr_time_us = duration.trunc_to_plain_us().value;

	_alarm__handle(curr_time_us);

	/* sleep time is either until the next deadline or the maximum timout */
	unsigned long sleep_time_us;
	Alarm::Time deadline_us;
	if (_alarm__next_deadline(&deadline_us)) {
		sleep_time_us = deadline_us - curr_time_us;
	} else {
		sleep_time_us = _time_source.max_timeout().value; }

	/* limit max timeout to a more reasonable value, e.g. 60s */
	if (sleep_time_us > 60000000) {
		sleep_time_us = 60000000;
	} else if (sleep_time_us == 0) {
		sleep_time_us = 1; }

	_time_source.schedule_timeout(Microseconds(sleep_time_us), *this);
}


Alarm_timeout_scheduler::Alarm_timeout_scheduler(Time_source  &time_source,
                                                 Microseconds  min_handle_period)
:
	_time_source(time_source)
{
	Alarm::Time const deadline         = _now + min_handle_period.value;
	_min_handle_period.period          = min_handle_period.value;
	_min_handle_period.deadline        = deadline;
	_min_handle_period.deadline_period = _now > deadline ?
	                                     !_now_period : _now_period;
}


Alarm_timeout_scheduler::~Alarm_timeout_scheduler()
{
	Lock::Guard lock_guard(_lock);
	while (_head) {
		Alarm *next = _head->_next;
		_head->_alarm__reset();
		_head = next;
	}
}


void Alarm_timeout_scheduler::_enable()
{
	_time_source.schedule_timeout(Microseconds(0), *this);
}


void Alarm_timeout_scheduler::_schedule_one_shot(Timeout      &timeout,
                                                 Microseconds  duration)
{
	unsigned long const curr_time_us =
		_time_source.curr_time().trunc_to_plain_us().value;

	/* ensure that the schedulers time is up-to-date before adding a timeout */
	_alarm__handle(curr_time_us);
	_alarm__schedule_absolute(&timeout._alarm, duration.value);

	if (_alarm__head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}


void Alarm_timeout_scheduler::_schedule_periodic(Timeout      &timeout,
                                                 Microseconds  duration)
{
	/* ensure that the schedulers time is up-to-date before adding a timeout */
	_alarm__handle(_time_source.curr_time().trunc_to_plain_us().value);
	_alarm__schedule(&timeout._alarm, duration.value);

	if (_alarm__head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}


void Alarm_timeout_scheduler::_alarm__unsynchronized_enqueue(Alarm *alarm)
{
	if (alarm->_active) {
		error("trying to insert the same alarm twice!");
		return;
	}

	alarm->_active++;

	/* if alarmlist is empty add first element */
	if (!_head) {
		alarm->_next = 0;
		_head = alarm;
		return;
	}

	/* if deadline is smaller than any other deadline, put it on the head */
	if (alarm->_raw.is_pending_at(_head->_raw.deadline, _head->_raw.deadline_period)) {
		alarm->_next = _head;
		_head = alarm;
		return;
	}

	/* find list element with a higher deadline */
	Alarm *curr = _head;
	while (curr->_next &&
	       curr->_next->_raw.is_pending_at(alarm->_raw.deadline, alarm->_raw.deadline_period))
	{
		curr = curr->_next;
	}

	/* if end of list is reached, append new element */
	if (curr->_next == 0) {
		curr->_next = alarm;
		return;
	}

	/* insert element in middle of list */
	alarm->_next = curr->_next;
	curr->_next  = alarm;
}


void Alarm_timeout_scheduler::_alarm__unsynchronized_dequeue(Alarm *alarm)
{
	if (!_head) return;

	if (_head == alarm) {
		_head = alarm->_next;
		alarm->_alarm__reset();
		return;
	}

	/* find predecessor in alarm queue */
	Alarm *curr;
	for (curr = _head; curr && (curr->_next != alarm); curr = curr->_next);

	/* alarm is not enqueued */
	if (!curr) return;

	/* remove alarm from alarm queue */
	curr->_next = alarm->_next;
	alarm->_alarm__reset();
}


Timeout::Alarm *Alarm_timeout_scheduler::_alarm__get_pending_alarm()
{
	Lock::Guard lock_guard(_lock);

	if (!_head || !_head->_raw.is_pending_at(_now, _now_period)) {
		return nullptr; }

	/* remove alarm from head of the list */
	Alarm *pending_alarm = _head;
	_head = _head->_next;

	/*
	 * Acquire dispatch lock to defer destruction until the call of '_on_alarm'
	 * is finished
	 */
	pending_alarm->_dispatch_lock.lock();

	/* reset alarm object */
	pending_alarm->_next = nullptr;
	pending_alarm->_active--;

	return pending_alarm;
}


void Alarm_timeout_scheduler::_alarm__handle(Alarm::Time curr_time)
{
	/*
	 * Raise the time counter and if it wraps, update also in which
	 * period of the time counter we are.
	 */
	if (_now > curr_time) {
		_now_period = !_now_period;
	}
	_now = curr_time;

	if (!_min_handle_period.is_pending_at(_now, _now_period)) {
		return;
	}
	Alarm::Time const deadline         = _now + _min_handle_period.period;
	_min_handle_period.deadline        = deadline;
	_min_handle_period.deadline_period = _now > deadline ?
	                                     !_now_period : _now_period;

	Alarm *curr;
	while ((curr = _alarm__get_pending_alarm())) {

		unsigned long triggered = 1;

		if (curr->_raw.period) {
			Alarm::Time deadline = curr->_raw.deadline;

			/* schedule next event */
			if (deadline == 0)
				 deadline = curr_time;

			triggered += (curr_time - deadline) / curr->_raw.period;
		}

		/* do not reschedule if alarm function returns 0 */
		bool reschedule = curr->_on_alarm(triggered);

		if (reschedule) {

			/*
			 * At this point, the alarm deadline normally is somewhere near
			 * the current time but If the alarm had no deadline by now,
			 * initialize it with the current time.
			 */
			if (curr->_raw.deadline == 0) {
				curr->_raw.deadline        = _now;
				curr->_raw.deadline_period = _now_period;
			}
			/*
			 * Raise the deadline value by one period of the alarm and
			 * if the deadline value wraps thereby, update also in which
			 * period it is located.
			 */
			Alarm::Time const deadline = curr->_raw.deadline +
			                             triggered * curr->_raw.period;
			if (curr->_raw.deadline > deadline) {
				curr->_raw.deadline_period = !curr->_raw.deadline_period;
			}
			curr->_raw.deadline = deadline;

			/* synchronize enqueue operation */
			Lock::Guard lock_guard(_lock);
			_alarm__unsynchronized_enqueue(curr);
		}

		/* release alarm, resume concurrent destructor operation */
		curr->_dispatch_lock.unlock();
	}
}


void Alarm_timeout_scheduler::_alarm__setup_alarm(Alarm &alarm, Alarm::Time period, Alarm::Time first_duration)
{
	/*
	 * If the alarm is already present in the queue, re-consider its queue
	 * position because its deadline might have changed. I.e., if an alarm is
	 * rescheduled with a new timeout before the original timeout triggered.
	 */
	if (alarm._active)
		_alarm__unsynchronized_dequeue(&alarm);

	Alarm::Time deadline = _now + first_duration;
	alarm._alarm__assign(period, deadline, _now > deadline ? !_now_period : _now_period, this);

	_alarm__unsynchronized_enqueue(&alarm);
}


void Alarm_timeout_scheduler::_alarm__schedule_absolute(Alarm *alarm, Alarm::Time duration)
{
	Lock::Guard alarm_list_lock_guard(_lock);

	_alarm__setup_alarm(*alarm, 0, duration);
}


void Alarm_timeout_scheduler::_alarm__schedule(Alarm *alarm, Alarm::Time period)
{
	Lock::Guard alarm_list_lock_guard(_lock);

	/*
	 * Refuse to schedule a periodic timeout of 0 because it would trigger
	 * infinitely in the 'handle' function. To account for the case where the
	 * alarm object was already scheduled, we make sure to remove it from the
	 * queue.
	 */
	if (period == 0) {
		_alarm__unsynchronized_dequeue(alarm);
		return;
	}

	/* first deadline is overdue */
	_alarm__setup_alarm(*alarm, period, 0);
}


void Alarm_timeout_scheduler::_alarm__discard(Alarm *alarm)
{
	/*
	 * Make sure that nobody is inside the '_alarm__get_pending_alarm' when
	 * grabbing the '_dispatch_lock'. This is important when this function
	 * is called from the 'Alarm' destructor. Without the '_dispatch_lock',
	 * we could take the lock and proceed with destruction just before
	 * '_alarm__get_pending_alarm' tries to grab the lock. When the destructor is
	 * finished, '_alarm__get_pending_alarm' would proceed with operating on a
	 * dangling pointer.
	 */
	Lock::Guard alarm_list_lock_guard(_lock);

	if (alarm) {
		Lock::Guard alarm_lock_guard(alarm->_dispatch_lock);
		_alarm__unsynchronized_dequeue(alarm);
	}
}


bool Alarm_timeout_scheduler::_alarm__next_deadline(Alarm::Time *deadline)
{
	Lock::Guard alarm_list_lock_guard(_lock);

	if (!_head) return false;

	if (deadline)
		*deadline = _head->_raw.deadline;

	if (*deadline < _min_handle_period.deadline) {
		*deadline = _min_handle_period.deadline;
	}
	return true;
}
