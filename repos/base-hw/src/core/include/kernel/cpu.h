/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__CPU_H_
#define _KERNEL__CPU_H_

/* core includes */
#include <timer.h>
#include <cpu.h>
#include <kernel/cpu_scheduler.h>
#include <kernel/perf_counter.h>

/* base includes */
#include <unmanaged_singleton.h>

namespace Kernel
{
	/**
	 * Context of a job (thread, VM, idle) that shall be executed by a CPU
	 */
	class Cpu_job;

	/**
	 * Ability to do a domain update on all CPUs
	 */
	class Cpu_domain_update;

	/**
	 * Execution context that is scheduled on CPU idle
	 */
	class Cpu_idle;

	/**
	 * Class for kernel data that is needed to manage a specific CPU
	 */
	class Cpu;

	/**
	 * Provides a CPU object for every available CPU
	 */
	class Cpu_pool;

	/**
	 * Return singleton of CPU pool
	 */
	Cpu_pool * cpu_pool();
}

class Kernel::Cpu_domain_update : public Double_list_item
{
	friend class Cpu_domain_update_list;

	private:

		bool     _pending[NR_OF_CPUS];
		unsigned _domain_id;

		/**
		 * Domain-update back-end
		 */
		void _domain_update() { Genode::Cpu::flush_tlb_by_pid(_domain_id); }

		/**
		 * Perform the domain update on the executing CPU
		 */
		void _do();

	protected:

		/**
		 * Constructor
		 */
		Cpu_domain_update()
		{
			for (unsigned i = 0; i < NR_OF_CPUS; i++) { _pending[i] = false; }
		}

		/**
		 * Do an update of domain 'id' on all CPUs and return if this blocks
		 */
		bool _do_global(unsigned const id);

		/**
		 * Notice that the update isn't pending on any CPU anymore
		 */
		virtual void _cpu_domain_update_unblocks() = 0;
};

class Kernel::Cpu_job : public Cpu_share
{
	protected:

		Cpu *            _cpu;
		Cpu_lazy_state   _lazy_state;
		Genode::uint64_t _debug_cycles_payed;
		Genode::uint64_t _debug_cycles_executed;

		/**
		 * Handle interrupt exception that occured during execution on CPU 'id'
		 */
		void _interrupt(unsigned const id);

		/**
		 * Insert context into the scheduling of this CPU
		 */
		void _schedule();

		/**
		 * Remove context from the scheduling of this CPU
		 */
		void _unschedule();

		/**
		 * Yield the currently scheduled CPU share of this context
		 */
		void _yield();

	public:

		/**
		 * Handle exception that occured during execution on CPU 'id'
		 */
		virtual void exception(unsigned const id) = 0;

		/**
		 * Continue execution on CPU 'id'
		 */
		virtual void proceed(unsigned const id) = 0;

		/**
		 * Construct a job with scheduling priority 'p' and time quota 'q'
		 */
		Cpu_job(Cpu_priority const p, unsigned const q)
		:
			Cpu_share(p, q), _cpu(0), _debug_cycles_payed(0),
			_debug_cycles_executed(0)
		{ }

		/**
		 * Destructor
		 */
		~Cpu_job();

		/**
		 * Link job to CPU 'cpu'
		 */
		void affinity(Cpu * const cpu);


		/***************
		 ** Cpu_share **
		 ***************/

		virtual void print_cpu_share_id() = 0;

		void debug_payed(unsigned const cycles)
		{
			if (cycles > (Genode::uint64_t)~0 - _debug_cycles_payed) {
				PWRN("overflow in cycle counter of job"); }
			_debug_cycles_payed += cycles;
		}

		void debug_executed(unsigned const cycles)
		{
			if (cycles > (Genode::uint64_t)~0 - _debug_cycles_executed) {
				PWRN("overflow in cycle counter of job"); }
			_debug_cycles_executed += cycles;
		}

		/***************
		 ** Accessors **
		 ***************/

		void cpu(Cpu * const cpu) { _cpu = cpu; }
		Cpu_lazy_state * lazy_state() { return &_lazy_state; }
};

class Kernel::Cpu_idle : public Genode::Cpu::User_context, public Cpu_job
{
	private:

		static constexpr size_t stack_size = sizeof(addr_t) * 32;

		char _stack[stack_size] __attribute__((aligned(16)));

		/**
		 * Main function of all idle threads
		 */
		static void _main() { while (1) { Genode::Cpu::wait_for_interrupt(); } }

	public:

		/**
		 * Construct idle context for CPU 'cpu'
		 */
		Cpu_idle(Cpu * const cpu);

		/**
		 * Handle exception that occured during execution on CPU 'cpu'
		 */
		void exception(unsigned const cpu)
		{
			switch (cpu_exception) {
			case INTERRUPT_REQUEST:      _interrupt(cpu); return;
			case FAST_INTERRUPT_REQUEST: _interrupt(cpu); return;
			case RESET:                                   return;
			default: assert(0); }
		}

		/**
		 * Continue execution on CPU 'cpu_id'
		 */
		void proceed(unsigned const cpu_id);


		/***************
		 ** Cpu_share **
		 ***************/

		void print_cpu_share_id();
};

class Kernel::Cpu : public Genode::Cpu
{
	private:

		typedef Cpu_job Job;

		unsigned const _id;
		Cpu_idle       _idle;
		Timer * const  _timer;
		Cpu_scheduler  _scheduler;
		bool           _ip_interrupt_pending;

		unsigned _quota() const { return _timer->ms_to_tics(cpu_quota_ms); }
		unsigned _fill() const { return _timer->ms_to_tics(cpu_fill_ms); }
		Job * _paying_job() const {
			return static_cast<Job *>(_scheduler.head()); }
		Job * _executing_job() const { return _paying_job(); }

		void _debug_update(Job * const paying, Job * const executing)
		{
			static unsigned perf_counter_last[NR_OF_CPUS];
			static bool init = 0;
			if (!init) {
				for (unsigned cpu = 0; cpu < NR_OF_CPUS; cpu++) {
					perf_counter_last[cpu] = 0; }
				init = 1;
			}
			bool o;
			unsigned v0 = perf_counter_last[Cpu::executing_id()];
			unsigned v1 = perf_counter()->value(o);
			unsigned v2;
			if (o) { v2 = (perf_counter()->max_value() - v0) + v1 ; }
			else { v2 = v1 - v0; }
			perf_counter_last[Cpu::executing_id()] = v1;
			paying->debug_payed(v2);
			executing->debug_executed(v2);
		}

	public:

		/**
		 * Construct object for CPU 'id' with scheduling timer 'timer'
		 */
		Cpu(unsigned const id, Timer * const timer)
		:
			_id(id), _idle(this), _timer(timer),
			_scheduler(&_idle, _quota(), _fill()),
			_ip_interrupt_pending(false) { }

		/**
		 * Check if IRQ 'i' was due to a scheduling timeout
		 */
		bool timer_irq(unsigned const i) { return _timer->interrupt_id(_id) == i; }

		/**
		 * Notice that the IPI of the CPU isn't pending anymore
		 */
		void ip_interrupt_handled() { _ip_interrupt_pending = false; }

		/**
		 * Raise the IPI of the CPU
		 */
		void trigger_ip_interrupt();

		/**
		 * Schedule 'job' at this CPU
		 */
		void schedule(Job * const job);

		/**
		 * Handle recent exception of the CPU and proceed its user execution
		 */
		void exception()
		{
			/* update old job */
			Job * const old_job = _executing_job();
//			_debug_update(_paying_job(), old_job);
//			perf_counter()->pause();
			old_job->exception(_id);

			/* update scheduler */
			unsigned const old_time = _scheduler.head_quota();
			unsigned const new_time = _timer->value(_id);
			unsigned quota = old_time > new_time ? old_time - new_time : 1;
			_scheduler.update(quota);

			/* get new job */
			Job * const new_job = _executing_job();
			quota = _scheduler.head_quota();
			assert(quota);
			_timer->start_one_shot(quota, _id);

			/* switch between lazy state of old and new job */
			Cpu_lazy_state * const old_state = old_job->lazy_state();
			Cpu_lazy_state * const new_state = new_job->lazy_state();
			prepare_proceeding(old_state, new_state);

			bool o;
			unsigned volatile v0 = perf_counter()->value(o);

			static unsigned volatile xxx[1024];
			for (unsigned volatile j = 0; j < 1024; j++) {
				for (unsigned volatile i = 0; i < 1024; i++) { xxx[i] = j; }
			}

			unsigned volatile v1 = perf_counter()->value(o);
			unsigned volatile v2;
			if (o) { v2 = (perf_counter()->max_value() - v0) + v1 ; }
			else { v2 = v1 - v0; }
			Genode::printf("mc %x\n", v2);

//perf_counter()->resume();
			/* resume new job */
			new_job->proceed(_id);
		}

		/***************
		 ** Accessors **
		 ***************/

		unsigned id() const { return _id; }
		Cpu_scheduler * scheduler() { return &_scheduler; }
};

class Kernel::Cpu_pool
{
	private:

		Timer _timer;
		char  _cpus[NR_OF_CPUS][sizeof(Cpu)];

	public:

		/**
		 * Construct pool and thereby objects for all available CPUs
		 */
		Cpu_pool()
		{
			for (unsigned id = 0; id < NR_OF_CPUS; id++) {
				new (_cpus[id]) Cpu(id, &_timer); }
		}

		/**
		 * Return object of CPU 'id'
		 */
		Cpu * cpu(unsigned const id) const
		{
			assert(id < NR_OF_CPUS);
			char * const p = const_cast<char *>(_cpus[id]);
			return reinterpret_cast<Cpu *>(p);
		}

		/**
		 * Return object of primary CPU
		 */
		Cpu * primary_cpu() const { return cpu(Cpu::primary_id()); }

		/*
		 * Accessors
		 */

		Timer * timer() { return &_timer; }
};

#endif /* _KERNEL__CPU_H_ */
