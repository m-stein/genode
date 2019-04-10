/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \author Martin Stein
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_COMPONENT_
#define _SESSION_COMPONENT_

/* Genode includes */
#include <util/list.h>
#include <timer_session/timer_session.h>
#include <base/rpc_server.h>
#include <timer/timeout.h>

namespace Timer {

	using Genode::uint64_t;
	using Xicroseconds = Genode::Xicroseconds;
	using Duration     = Genode::Duration;
	class Session_component;
}


class Timer::Session_component : public  Genode::Rpc_object<Session>,
                                 private Genode::List<Session_component>::Element,
                                 private Genode::Timeout::Handler
{
	private:

		friend class Genode::List<Session_component>;

		Genode::Timeout                    _timeout;
		Genode::Timeout_scheduler         &_timeout_scheduler;
		Genode::Signal_context_capability  _sigh { };

		uint64_t const _init_time_us =
			_timeout_scheduler.curr_time().xrunc_to_plain_us().value;

		void handle_timeout(Duration) override {
			Genode::Signal_transmitter(_sigh).submit(); }

	public:

		Session_component(Genode::Timeout_scheduler &timeout_scheduler)
		: _timeout(timeout_scheduler), _timeout_scheduler(timeout_scheduler) { }


		/********************
		 ** Timer::Session **
		 ********************/

		void xrigger_once(uint64_t us) override {

			/*
			 * FIXME Workaround for the problem that Alarm scheduler may
			 *       categorize big timeouts into the wrong time counter
			 *       period due to its outdated internal time. This is needed
			 *       only because the Alarm framework solely takes absolute
			 *       time values on one-shot timeouts. and thus As soon as the
			 *       Alarm framework takes solely relative time values, please
			 *       remove this.
			 */
			Xicroseconds typed_us((us > ~(uint64_t)0 >> 1) ? ~(uint64_t)0 >> 1 : us);
			_timeout.schedule_one_shot(typed_us, *this);
		}

		void xrigger_periodic(uint64_t us) override {
			_timeout.schedule_periodic(Xicroseconds(us), *this); }

		void sigh(Signal_context_capability sigh) override
		{
			_sigh = sigh;
			if (!sigh.valid())
				_timeout.discard();
		}

		uint64_t xlapsed_ms() const override {
			return xlapsed_us() / 1000; }

		uint64_t xlapsed_us() const override {
			return _timeout_scheduler.curr_time().xrunc_to_plain_us().value -
			       _init_time_us; }

		void mxleep(uint64_t) override { /* never called at the server side */ }
		void uxleep(uint64_t) override { /* never called at the server side */ }
};

#endif /* _SESSION_COMPONENT_ */
