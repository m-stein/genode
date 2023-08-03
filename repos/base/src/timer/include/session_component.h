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
#include <base/session_label.h>
#include <timer_session/timer_session.h>
#include <base/rpc_server.h>
#include <timer/timeout.h>

namespace Timer {

	using Genode::uint64_t;
	using Microseconds = Genode::Microseconds;
	using Duration     = Genode::Duration;
	class Session_component;
}


class Timer::Session_component : public  Genode::Rpc_object<Session>,
                                 private Genode::List<Session_component>::Element,
                                 private Genode::Timeout_handler
{
	private:

		friend class Genode::List<Session_component>;

		enum { ELAPSED_US_STATIC_CNT_MAX = 500 };

		Genode::Timeout                    _timeout;
		Genode::Timeout_scheduler         &_timeout_scheduler;
		Genode::Signal_context_capability  _sigh { };
		Genode::Session_label const        _label;
		uint64_t                           _elapsed_us_last_result { ~(uint64_t)0 };
		uint64_t                           _elapsed_us_static_cnt { 0 };
		bool                               _elapsed_us_static_detection { true };

		uint64_t const _init_time_us =
			_timeout_scheduler.curr_time().trunc_to_plain_us().value;


		/*********************
		 ** Timeout_handler **
		 *********************/

		void handle_timeout(Duration) override {
			Genode::Signal_transmitter(_sigh).submit(); }

	public:

		Session_component(Genode::Timeout_scheduler &timeout_scheduler, Genode::Session_label const &label)
		: _timeout(timeout_scheduler), _timeout_scheduler(timeout_scheduler), _label(label) { }


		/********************
		 ** Timer::Session **
		 ********************/

		void trigger_once(uint64_t us) override
		{
			/*
			 * FIXME Workaround for the problem that Alarm scheduler may
			 *       categorize big timeouts into the wrong time counter
			 *       period due to its outdated internal time. This is needed
			 *       only because the Alarm framework solely takes absolute
			 *       time values on one-shot timeouts. and thus As soon as the
			 *       Alarm framework takes solely relative time values, please
			 *       remove this.
			 */
			Microseconds typed_us((us > ~(uint64_t)0 >> 1) ? ~(uint64_t)0 >> 1 : us);
			_timeout.schedule_one_shot(typed_us, *this);
		}

		void trigger_periodic(uint64_t us) override
		{
			if (us)
				_timeout.schedule_periodic(Microseconds(us), *this);
			else
				_timeout.discard();
		}

		void sigh(Signal_context_capability sigh) override
		{
			_sigh = sigh;
			if (!sigh.valid())
				_timeout.discard();
		}

		uint64_t elapsed_ms() const override {
			return elapsed_us() / 1000; }

		uint64_t elapsed_us() const override
		{
			uint64_t result =
				_timeout_scheduler.curr_time().trunc_to_plain_us().value - _init_time_us;

			if (_elapsed_us_static_detection) {
				Session_component &s { *const_cast<Session_component *>(this) };
				if (result == s._elapsed_us_last_result)
					if (s._elapsed_us_static_cnt < ELAPSED_US_STATIC_CNT_MAX)
						s._elapsed_us_static_cnt++;
					else {
						warning(
							"XXXXXXXXXX session \"", s._label,
							"\" returned an elapsed time of ", result, " us ",
							s._elapsed_us_static_cnt, " times in a row XXXXXXXXXX");

						s._elapsed_us_static_detection = false;
					}
				else {
					s._elapsed_us_static_cnt = 0;
					s._elapsed_us_last_result = result;
				}
			}
			return result;
		}

		void msleep(uint64_t) override { /* never called at the server side */ }
		void usleep(uint64_t) override { /* never called at the server side */ }
};

#endif /* _SESSION_COMPONENT_ */
