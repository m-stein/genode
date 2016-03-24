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

#ifndef _CORE__INCLUDE__KERNEL__CLOCK_H_
#define _CORE__INCLUDE__KERNEL__CLOCK_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/list.h>

/* Core includes */
#include <timer.h>

namespace Kernel
{
	using time_t = unsigned long;

	class Timeout;
	class Clock;
}

/**
 * A timeout causes a kernel pass and the call of a timeout specific handle
 */
class Kernel::Timeout : public Genode::List<Timeout>::Element
{
	friend class Clock;

	private:

		bool   _listed = false;
		time_t _start  = 0;
		time_t _end    = 0;

	public:

		/**
		 * Callback handle
		 */
		virtual void timeout_triggered() { }

		virtual ~Timeout() { }
};

/**
 * A clock manages a continuous time and timeouts on it
 */
class Kernel::Clock
{
	private:

		unsigned const        _cpu_id;
		Timer * const         _timer;
		time_t                _time = 0;
		Genode::List<Timeout> _timeout_list;
		duration_t            _last_timeout_duration = 0;

		void _insert_timeout(Timeout * const timeout);

		void _remove_timeout(Timeout * const timeout);

	public:

		Clock(unsigned const cpu_id, Timer * const timer);

		/**
		 * Set-up timer according to the current timeout schedule
		 */
		void schedule_timeout();

		/**
		 * Update time and work off expired timeouts
		 *
		 * \return  time that passed since the last scheduling
		 */
		duration_t update();

		/**
		 * Set-up 'timeout' to trigger at time + 'duration'
		 */
		void set_timeout(Timeout * const timeout, duration_t const duration);

		/**
		 * Return native time value that equals the given microseconds 'us'
		 */
		duration_t us_to_tics(duration_t const us) const;

		/**
		 * Return microseconds that passed since the last set-up of 'timeout'
		 */
		duration_t timeout_age_us(Timeout const * const timeout) const;

		duration_t timeout_max() const;
};

#endif /* _CORE__INCLUDE__KERNEL__CLOCK_H_ */
