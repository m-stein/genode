/*
 * \brief   A multiplexable common instruction processor
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

#ifndef _KERNEL__PROCESSOR_H_
#define _KERNEL__PROCESSOR_H_

/* core includes */
#include <processor_driver.h>
#include <kernel/scheduler.h>

#include <util/list.h>

namespace Kernel
{
	/**
	 * A single user of a multiplexable processor
	 */
	class Processor_client;

	/**
	 * Multiplexes a single processor to multiple processor clients
	 */
	typedef Scheduler<Processor_client> Processor_scheduler;

	/**
	 * A multiplexable common instruction processor
	 */
	class Processor;
}

class Kernel::Processor_client : public Processor_scheduler::Item
{
	protected:

		Processor * _processor;

		using List_item = Genode::List_element<Processor_client>;

		List_item _flush_tlb_li;     /* TLB maintainance work list item       */
		unsigned  _flush_tlb_pd_id;  /* id of pd that TLB entries are flushed */
		bool      _flush_tlb_ref_cnt[PROCESSORS]; /* reference counters */

		/**
		 * Handle an interrupt exception that occured during execution
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		void _interrupt(unsigned const processor_id);

		/**
		 * Insert context into the processor scheduling
		 */
		void _schedule();

		/**
		 * Remove context from the processor scheduling
		 */
		void _unschedule();

		/**
		 * Yield currently scheduled processor share of the context
		 */
		void _yield();

	public:

		/**
		 * Handle an exception that occured during execution
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		virtual void exception(unsigned const processor_id) = 0;

		/**
		 * Continue execution
		 *
		 * \param processor_id  kernel name of targeted processor
		 */
		virtual void proceed(unsigned const processor_id) = 0;

		/**
		 * Enqueues TLB maintainance work into queue of the processors
		 *
		 * \param pd_id  protection domain kernel object's id
		 */
		void tlb_to_flush(unsigned pd_id);

		/**
		 * Flush TLB entries requested by this client on the current processor
		 */
		void flush_tlb_by_id();

		/**
		 * Constructor
		 *
		 * \param processor  kernel object of targeted processor
		 * \param priority   scheduling priority
		 */
		Processor_client(Processor * const processor, Priority const priority)
		:
			Processor_scheduler::Item(priority),
			_processor(processor),
			_flush_tlb_li(this)
		{ }

		/**
		 * Destructor
		 */
		~Processor_client()
		{
			if (!_scheduled()) { return; }
			_unschedule();
		}
};

class Kernel::Processor : public Processor_driver
{
	private:

		unsigned const      _id;
		Processor_scheduler _scheduler;
		bool                _ip_interrupt_pending;

	public:

		/**
		 * Constructor
		 *
		 * \param id           kernel name of the processor object
		 * \param idle_client  client that gets scheduled on idle
		 */
		Processor(unsigned const id, Processor_client * const idle_client)
		:
			_id(id), _scheduler(idle_client), _ip_interrupt_pending(false)
		{ }

		/**
		 * Perform outstanding TLB maintainance work
		 */
		void flush_tlb();

		/**
		 * Notice that the inter-processor interrupt isn't pending anymore
		 */
		void ip_interrupt()
		{
			/*
			 * This interrupt solely denotes that another processor has
			 * modified the scheduling plan of this processor and thus
			 * a more prior user context than the current one might be
			 * available.
			 */
			_ip_interrupt_pending = false;
			flush_tlb();
		}

		/**
		 * Add a processor client to the scheduling plan of the processor
		 *
		 * \param client  targeted client
		 */
		void schedule(Processor_client * const client);


		/***************
		 ** Accessors **
		 ***************/

		unsigned id() const { return _id; }

		Processor_scheduler * scheduler() { return &_scheduler; }
};

#endif /* _KERNEL__PROCESSOR_H_ */
