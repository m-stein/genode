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

namespace Kernel
{
	using Genode::Processor_driver;
	using Genode::Processor_lazy_state;

	/**
	 * A single user of a multiplexable processor
	 */
	class Processor_client;

	/**
	 * Ability to broadcast a method to all processors
	 */
	class Processor_broadcast;

	/**
	 * Multiplexes a single processor to multiple processor clients
	 */
	typedef Scheduler<Processor_client> Processor_scheduler;

	/**
	 * A multiplexable common instruction processor
	 */
	class Processor;
}

class Kernel::Processor_broadcast
:
	public Double_list_item<Processor_broadcast>
{
	friend class Processor_broadcast_list;

	private:

		bool _pending[PROCESSORS];

		/**
		 * Perform the broadcast method on the executing processors
		 */
		void _perform_locally();

	protected:

		/**
		 * Constructor
		 */
		Processor_broadcast()
		{
			for (unsigned i = 0; i < PROCESSORS; i++) { _pending[i] = false; }
		}

		/**
		 * Perform the broadcast method on all processors
		 */
		void _perform();

		/**
		 * Method that shall be performed on all processors by the broadcast
		 */
		virtual void _processor_broadcast_method() = 0;

		/**
		 * Notice that the broadcast isn't pending on any processor anymore
		 */
		virtual void _processor_broadcast_unblocks() = 0;

		/**
		 * Notice that the broadcast is pending on other processors
		 */
		virtual void _processor_broadcast_blocks() = 0;
};

class Kernel::Processor_client : public Processor_scheduler::Item
{
	protected:

		Processor *          _processor;
		Processor_lazy_state _lazy_state;

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
		 * Constructor
		 *
		 * \param processor  kernel object of targeted processor
		 * \param priority   scheduling priority
		 */
		Processor_client(Processor * const processor, Priority const priority)
		:
			Processor_scheduler::Item(priority),
			_processor(processor)
		{ }

		/**
		 * Destructor
		 */
		~Processor_client()
		{
			if (!_scheduled()) { return; }
			_unschedule();
		}


		/***************
		 ** Accessors **
		 ***************/

		Processor_lazy_state * lazy_state() { return &_lazy_state; }
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
		 * Notice that the inter-processor interrupt isn't pending anymore
		 */
		void ip_interrupt_handled() { _ip_interrupt_pending = false; }

		/**
		 * Raise the inter-processor interrupt of the processor
		 */
		void trigger_ip_interrupt();

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
