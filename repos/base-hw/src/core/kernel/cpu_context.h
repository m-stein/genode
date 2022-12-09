/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__CPU_CONTEXT_H_
#define _CORE__KERNEL__CPU_CONTEXT_H_

/* core includes */
#include <kernel/scheduler.h>
#include <kernel/timer.h>

namespace Kernel {

	class Cpu;

	/**
	 * Context of a job (thread, VM, idle) that shall be executed by a CPU
	 */
	class Cpu_job;
}


class Kernel::Cpu_job : private Scheduling_context
{
	private:

		friend class Cpu; /* static_cast from 'Scheduling_context' to 'Cpu_job' */

		time_t _execution_time { 0 };

		/*
		 * Noncopyable
		 */
		Cpu_job(Cpu_job const &);
		Cpu_job &operator = (Cpu_job const &);

	protected:

		Cpu * _cpu;

		/**
		 * Handle interrupt exception that occured during execution on CPU 'id'
		 */
		void _interrupt(Irq::Pool &user_irq_pool, unsigned const id);

		/**
		 * Activate our own scheduling context
		 */
		void _activate_own_sched_context();

		/**
		 * Deactivate our own scheduling context
		 */
		void _deactivate_own_sched_context();

		/**
		 * Yield the currently scheduled scheduling context of this job
		 */
		void _yield();

		/**
		 * Return wether we are allowed to help job 'j' with our scheduling context
		 */
		bool _helping_possible(Cpu_job const &j) const { return j._cpu == _cpu; }

	public:

		/**
		 * Handle exception that occured during execution on CPU 'id'
		 */
		virtual void exception(Cpu & cpu) = 0;

		/**
		 * Continue execution on CPU 'id'
		 */
		virtual void proceed(Cpu & cpu) = 0;

		/**
		 * Return which job currently uses our scheduling context
		 */
		virtual Cpu_job * helping_sink() = 0;

		/**
		 * Construct a job with scheduling priority 'p' and time quota 'q'
		 */
		Cpu_job(Priority const p, unsigned const q);

		/**
		 * Destructor
		 */
		virtual ~Cpu_job();

		/**
		 * Link job to CPU 'cpu'
		 */
		void affinity(Cpu &cpu);

		/**
		 * Set CPU quota of the job to 'q'
		 */
		void quota(unsigned const q);

		/**
		 * Return wether our scheduling context is currently active
		 */
		bool own_sched_context_active() { return Scheduling_context::ready(); }

		/**
		 * Update total execution time
		 */
		void update_execution_time(time_t duration) { _execution_time += duration; }

		/**
		 * Return total execution time
		 */
		time_t execution_time() const { return _execution_time; }


		/***************
		 ** Accessors **
		 ***************/

		void cpu(Cpu &cpu) { _cpu = &cpu; }

		Scheduling_context &scheduling_context() { return *this; }
};

#endif /* _CORE__KERNEL__CPU_CONTEXT_H_ */
