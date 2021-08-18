/*
 * \brief   Schedules CPU shares for the execution time of a CPU
 * \author  Martin Stein
 * \date    2014-10-09
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CPU_SCHEDULER_H_
#define _CORE__KERNEL__CPU_SCHEDULER_H_

/* core includes */
#include <util/misc_math.h>
#include <kernel/configuration.h>
#include <kernel/ada_interfacing.h>

namespace Kernel
{
	class Cpu_priority;

	class Cpu_share;

	class Cpu_scheduler;
}


class Kernel::Cpu_priority
{
	private:

		unsigned _value;

	public:

		static constexpr unsigned min() { return 0; }
		static constexpr unsigned max() { return cpu_priorities - 1; }

		/**
		 * Construct priority with value 'v'
		 */
		Cpu_priority(unsigned const v)
		:
			_value { Genode::min(v, max()) }
		{ }

		/*
		 * Standard operators
		 */

		Cpu_priority &operator =(unsigned const v)
		{
			_value = Genode::min(v, max());
			return *this;
		}

		operator unsigned() const { return _value; }
};


struct Kernel::Cpu_share : Opaque_ada_type<Cpu_share, 88>
{
	Cpu_share(unsigned const p, unsigned const q);

	bool ready() const;
	void quota(unsigned const q);
};


struct Kernel::Cpu_scheduler : Opaque_ada_type<Cpu_scheduler, 216>
{
	Cpu_scheduler(Cpu_share &i, unsigned const q, unsigned const f);

	bool need_to_schedule();
	void timeout();

	void update(time_t time);

	bool ready_check(Cpu_share &s1);

	void ready(Cpu_share &s);

	void unready(Cpu_share &s);

	void yield();

	void remove(Cpu_share &s);

	void insert(Cpu_share &s);

	void quota(Cpu_share &s, unsigned const q);

	Cpu_share &head() const;
	unsigned head_quota() const;
};

#endif /* _CORE__KERNEL__CPU_SCHEDULER_H_ */
