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
	class Timeout;
	class Alarm_timeout_scheduler;
	template <typename> class Periodic_timeout;
	template <typename> class One_shot_timeout;
}


struct Genode::Timeout_scheduler
{
	using Microseconds = Time_source::Microseconds;

	virtual Microseconds curr_time() const = 0;

	virtual void schedule_one_shot(Timeout &timeout, Microseconds duration) = 0;

	virtual void schedule_periodic(Timeout &timeout, Microseconds duration) = 0;

	virtual void discard(Timeout &timeout) = 0;
};


class Genode::Timeout : private Noncopyable
{
	friend class Alarm_timeout_scheduler;

	public:

		struct Handler
		{
			using Microseconds = Time_source::Microseconds;

			virtual void handle_timeout(Microseconds curr_time) = 0;
		};

	private:

		using Microseconds = Time_source::Microseconds;


		struct Alarm : Genode::Alarm
		{
			Timeout_scheduler &timeout_scheduler;
			Handler           *handler = nullptr;
			bool               periodic;

			Alarm(Timeout_scheduler &timeout_scheduler)
			: timeout_scheduler(timeout_scheduler) { }


			/*******************
			 ** Genode::Alarm **
			 *******************/

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

		void schedule_periodic(Microseconds duration, Handler &handler)
		{
			_alarm.handler = &handler;
			_alarm.periodic = true;
			_alarm.timeout_scheduler.schedule_periodic(*this, duration);
		}

		void schedule_one_shot(Microseconds duration, Handler &handler)
		{
			_alarm.handler = &handler;
			_alarm.periodic = false;
			_alarm.timeout_scheduler.schedule_one_shot(*this, duration);
		}
};


template <typename HANDLER_TYPE>
struct Genode::Periodic_timeout : public Timeout::Handler
{
	private:

		typedef void (HANDLER_TYPE::*Handler_method)(Microseconds);

		Timeout               _timeout;
		HANDLER_TYPE         &_object;
		Handler_method const  _method;


		/**********************
		 ** Timeout::Handler **
		 **********************/

		void handle_timeout(Microseconds curr_time) override {
			(_object.*_method)(curr_time); }

	public:

		Periodic_timeout(Timeout_scheduler &timeout_scheduler,
		                 HANDLER_TYPE      &object,
		                 Handler_method     method,
		                 Microseconds       duration)
		:
			_timeout(timeout_scheduler), _object(object), _method(method)
		{
			_timeout.schedule_periodic(duration, *this);
		}
};


template <typename HANDLER_TYPE>
class Genode::One_shot_timeout : public Timeout::Handler
{
	private:

		typedef void (HANDLER_TYPE::*Handler_method)(Microseconds);

		Timeout               _timeout;
		HANDLER_TYPE         &_object;
		Handler_method const  _method;


		/**********************
		 ** Timeout::Handler **
		 **********************/

		void handle_timeout(Microseconds curr_time) override {
			(_object.*_method)(curr_time); }

	public:

		One_shot_timeout(Timeout_scheduler &timeout_scheduler,
		                 HANDLER_TYPE      &object,
		                 Handler_method     method)
		: _timeout(timeout_scheduler), _object(object), _method(method) { }

		void trigger(Microseconds duration) {
			_timeout.schedule_one_shot(duration, *this); }
};


class Genode::Alarm_timeout_scheduler : private Noncopyable,
                                        public  Timeout_scheduler,
                                        public  Time_source::Timeout_handler
{
	private:

		Time_source     &_time_source;
		Alarm_scheduler  _alarm_scheduler;


		/**********************************
		 ** Time_source::Timeout_handler **
		 **********************************/

		void handle_timeout(Microseconds curr_time) override
		{
			_alarm_scheduler.handle(curr_time.value);

			unsigned long sleep_time_us;
			Alarm::Time deadline_us;
			if (_alarm_scheduler.next_deadline(&deadline_us)) {
				sleep_time_us = deadline_us - curr_time.value;
			} else {
				sleep_time_us = _time_source.max_timeout().value; }

			if (sleep_time_us == 0) {
				sleep_time_us = 1; }

			_time_source.schedule_timeout(Microseconds(sleep_time_us), *this);
		}

	public:

		Alarm_timeout_scheduler(Time_source &time_source)
		: _time_source(time_source)
		{
			time_source.schedule_timeout(Microseconds(0), *this);
		}


		/***********************
		 ** Timeout_scheduler **
		 ***********************/

		Microseconds curr_time() const override {
			return _time_source.curr_time(); }

		void schedule_one_shot(Timeout &timeout, Microseconds duration) override
		{
			_alarm_scheduler.schedule_absolute(&timeout._alarm,
			                                   _time_source.curr_time().value +
			                                   duration.value);

			if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
				_time_source.schedule_timeout(Microseconds(0), *this); }
		}

		void schedule_periodic(Timeout &timeout, Microseconds duration) override
		{
			_alarm_scheduler.handle(_time_source.curr_time().value);
			_alarm_scheduler.schedule(&timeout._alarm, duration.value);
			if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
				_time_source.schedule_timeout(Microseconds(0), *this); }
		}

		void discard(Timeout &timeout) override {
			_alarm_scheduler.discard(&timeout._alarm); }
};

#endif /* _OS__TIMEOUT_H_ */
