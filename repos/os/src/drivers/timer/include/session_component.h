/*
 * \brief  Instance of the timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \author Martin Stein
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SESSION_COMPONENT_
#define _SESSION_COMPONENT_

/* Genode includes */
#include <util/list.h>
#include <timer_session/timer_session.h>
#include <base/rpc_server.h>
#include <os/timeout.h>

namespace Timer { class Session_component; }


class Timer::Session_component : public  Genode::Rpc_object<Session>,
                                 public  Genode::List<Session_component>::Element,
                                 private Genode::Timeout_handler
{
	private:

		Genode::Timeout                    _timeout;
		Genode::Timeout_scheduler         &_timeout_scheduler;
		Microseconds const                 _initial_time;
		Genode::Signal_context_capability  _sigh;

		void handle_timeout(Microseconds) { Genode::Signal_transmitter(_sigh).submit(); }

	public:

		Session_component(Genode::Timeout_scheduler &timeout_scheduler)
		:
			_timeout(timeout_scheduler),
			_timeout_scheduler(timeout_scheduler),
			_initial_time(_timeout_scheduler.curr_time())
		{ }


		/********************
		 ** Timer::Session **
		 ********************/

		void trigger_once(unsigned us) { _timeout.trigger_once(us, *this); }

		void trigger_periodic(unsigned us) { _timeout.trigger_periodic(us, *this); }

		void sigh(Signal_context_capability sigh) { _sigh = sigh; }

		unsigned long elapsed_ms() const {
			return (_timeout_scheduler.curr_time() - _initial_time) / 1000; }

		void msleep(unsigned) { /* never called at the server side */ }
		void usleep(unsigned) { /* never called at the server side */ }
};

#endif /* _SESSION_COMPONENT_ */
