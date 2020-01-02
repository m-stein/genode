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
#include <kernel/ada_object.h>

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

		enum {
			MIN = 0,
			MAX = cpu_priorities - 1,
		};

		Cpu_priority(signed const v) : _value(Genode::min(v, MAX)) { }

		Cpu_priority &operator =(signed const v)
		{
			_value = Genode::min(v, MAX);
			return *this;
		}

		operator signed() const { return _value; }
};

struct Kernel::Cpu_share : Ada_object<88>
{
	Cpu_share(signed const p, unsigned const q);

	bool ready() const;
	void quota(unsigned const q);
};

struct Kernel::Cpu_scheduler : Ada_object<216>
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
