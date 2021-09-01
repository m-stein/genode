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

#ifndef _CORE__KERNEL__TIMER_H_
#define _CORE__KERNEL__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>
#include <kernel/irq.h>

/* Genode includes */
#include <util/list.h>

#include <board.h>

namespace Kernel {

	class Cpu;
	class Timeout;
	using Timeout_list = Genode::List<Genode::List_element<Timeout> >;
	using Timeout_list_element = Genode::List_element<Timeout>;
	class Timer;
}


/**
 * A timeout causes a kernel pass and the call of a timeout specific handle
 */
class Kernel::Timeout
{
	friend class Timer;
	friend class Genode::List<Genode::List_element<Timeout> >;

	private:

		enum class Type
		{
			HANDLED_BY_THREAD,
			NOT_HANDLED
		};

		Type           const _type;
		Thread        *const _thread;
		Timeout_list_element _list_elem { this };
		bool                 _listed    { false };
		time_t               _end       { 0 };

	public:

		Timeout(Thread &thread);

		Timeout();

		void handle() const;
};


class Kernel::Timer_irq
{
	private:

		Kernel::Irq  _irq;
		Cpu         &_cpu;

	public:

		Timer_irq(unsigned  id,
		          Cpu      &cpu);

		void occurred();
};


/**
 * A timer manages a continuous time and timeouts on it
 */
class Kernel::Timer
{
	private:

		Board::Timer _device;
		Timer_irq    _irq;
		time_t       _time                        { 0 };
		time_t       _last_timeout_duration;
		time_t       _time_between_schedule_calls { 0 };
		Timeout_list _timeout_list                { };

		void _start_one_shot(time_t const ticks);

		time_t _max_value() const;

		time_t _duration() const;

	public:

		Timer(Cpu & cpu);

		void schedule_timeout();

		time_t time_between_schedule_calls() const;

		void process_timeouts();

		void set_timeout(Timeout * const timeout, time_t const duration);

		time_t us_to_ticks(time_t const us) const;

		time_t ticks_to_us(time_t const ticks) const;

		time_t timeout_max_us() const;

		unsigned interrupt_id() const;

		time_t time() const { return _time + _duration(); }
};

#endif /* _CORE__KERNEL__TIMER_H_ */
