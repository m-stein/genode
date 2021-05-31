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

namespace Kernel {

	class Cpu;
	class Timeout;
	class Timer;
}

struct Kernel::Timeout : Ada_object<56>
{
	Timeout(Thread &thread);

	Timeout();

	void handle() const;
};

struct Kernel::Timer : Ada_object<56>
{
	struct Irq : Kernel::Irq
	{
		Cpu & _cpu;

		Irq(unsigned id, Cpu & cpu);

		void occurred() override;
	};

	Genode::Constructible<Irq> _irq { };

	void _initialize();

	Timer(Cpu &cpu)
	{
		_initialize();
		_irq.construct(interrupt_id(), cpu);
	}

	void schedule_timeout();

	time_t time_between_schedule_calls() const;

	void process_timeouts();

	void set_timeout(Timeout * const timeout, time_t const duration);

	time_t us_to_ticks(time_t const us) const;

	time_t ticks_to_us(time_t const ticks) const;

	time_t timeout_max_us() const;

	unsigned interrupt_id() const;

	time_t time() const;
};

#endif /* _CORE__KERNEL__TIMER_H_ */
