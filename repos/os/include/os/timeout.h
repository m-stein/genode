/*
 * \brief  Multiplexes a time source amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OS__TIMEOUT_H_
#define _OS__TIMEOUT_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <os/time_source.h>
#include <os/alarm.h>

namespace Genode {

	class Timeout_scheduler;
	class Alarm_timeout_scheduler;
	class Timeout_handler;
	class Timeout;
	template <typename> class Periodic_timeout;
	template <typename> class One_shot_timeout;
}


struct Genode::Timeout_scheduler
{
	using Microseconds = Time_source::Microseconds;

	virtual Microseconds curr_time() const = 0;

	virtual void schedule_one_shot(Timeout            &timeout,
	                               Microseconds const  duration) = 0;

	virtual void schedule_periodic(Timeout            &timeout,
	                               Microseconds const  duration) = 0;

	virtual void discard(Timeout &timeout) = 0;
};


struct Genode::Timeout_handler
{
	using Microseconds = Time_source::Microseconds;

	virtual void handle_timeout(Microseconds curr_time) = 0;
};


class Genode::Timeout
{
	friend class Alarm_timeout_scheduler;

	private:

		using Microseconds = Time_source::Microseconds;


		struct Alarm : Genode::Alarm
		{
			Timeout_scheduler &timeout_scheduler;
			Timeout_handler   *handler = nullptr;
			bool               periodic;

			Alarm(Timeout_scheduler &timeout_scheduler)
			: timeout_scheduler(timeout_scheduler) { }


			/***********
			 ** Alarm **
			 ***********/

			bool on_alarm(unsigned) override
			{
				if (handler) {
					handler->handle_timeout(timeout_scheduler.curr_time()); }
				return periodic;
			}

		} _alarm;

	public:

		Timeout(Timeout_scheduler &timeout_scheduler)
		: _alarm(timeout_scheduler) { }

		~Timeout() { _alarm.timeout_scheduler.discard(*this); }

		void trigger_periodic(Microseconds duration, Timeout_handler &handler)
		{
			_alarm.handler = &handler;
			_alarm.periodic = true;
			_alarm.timeout_scheduler.schedule_periodic(*this, duration);
		}

		void trigger_once(Microseconds duration, Timeout_handler &handler)
		{
			_alarm.handler = &handler;
			_alarm.periodic = false;
			_alarm.timeout_scheduler.schedule_one_shot(*this, duration);
		}
};


template <typename HANDLER_TYPE>
struct Genode::Periodic_timeout : public Timeout_handler
{
	private:

		typedef void (HANDLER_TYPE::*Handler_method)(Microseconds);

		Timeout               _timeout;
		HANDLER_TYPE         &_object;
		Handler_method const  _method;

		void handle_timeout(Microseconds curr_time) { (_object.*_method)(curr_time); }

	public:

		Periodic_timeout(Timeout_scheduler  &timeout_scheduler,
		                 HANDLER_TYPE       &object,
		                 Handler_method      method,
		                 Microseconds const  duration)
		:
			_timeout(timeout_scheduler), _object(object), _method(method)
		{
			_timeout.trigger_periodic(duration, *this);
		}
};


template <typename HANDLER_TYPE>
class Genode::One_shot_timeout : public Timeout_handler
{
	private:

		typedef void (HANDLER_TYPE::*Handler_method)(Microseconds);

		Timeout               _timeout;
		HANDLER_TYPE         &_object;
		Handler_method const  _method;

		void handle_timeout(Microseconds curr_time) { (_object.*_method)(curr_time); }

	public:

		One_shot_timeout(Timeout_scheduler &timeout_scheduler,
		                 HANDLER_TYPE      &object,
		                 Handler_method     method)
		:
			_timeout(timeout_scheduler), _object(object), _method(method)
		{ }

		void trigger(Microseconds const duration)
		{
			_timeout.trigger_once(duration, *this);
		}
};


class Genode::Alarm_timeout_scheduler : private Noncopyable,
                                        public  Timeout_scheduler,
                                        public  Time_source::Timeout_handler
{
	private:

		Time_source     &_time_source;
		Alarm_scheduler  _alarm_scheduler;


		/*********************
		 ** Timeout_handler **
		 *********************/

		void handle_timeout(Microseconds curr_time) override
		{
			_alarm_scheduler.handle(curr_time);

			Microseconds sleep_time;
			Alarm::Time deadline;
			if (_alarm_scheduler.next_deadline(&deadline)) {
				sleep_time = deadline - curr_time;
			} else {
				sleep_time = _time_source.max_timeout(); }

			if (sleep_time == 0) {
				sleep_time = 1; }

			_time_source.schedule_timeout(sleep_time, *this);
		}

	public:

		Alarm_timeout_scheduler(Time_source &time_source)
		:
			_time_source(time_source)
		{
			_time_source.schedule_timeout(0, *this);
		}


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		Microseconds curr_time() const override
		{
			return _time_source.curr_time();
		}

		void schedule_one_shot(Timeout            &timeout,
		                       Microseconds const  duration) override
		{
			_alarm_scheduler.schedule_absolute(
				&timeout._alarm, _time_source.curr_time() + duration);
			if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
				_time_source.schedule_timeout(0, *this); }
		}

		void schedule_periodic(Timeout            &timeout,
		                       Microseconds const  duration) override
		{
			_alarm_scheduler.handle(_time_source.curr_time());
			_alarm_scheduler.schedule(&timeout._alarm, duration);
			if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
				_time_source.schedule_timeout(0, *this); }
		}

		void discard(Timeout &timeout) override
		{
			_alarm_scheduler.discard(&timeout._alarm);
		}
};

#endif /* _OS__TIMEOUT_H_ */
