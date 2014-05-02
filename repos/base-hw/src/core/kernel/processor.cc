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

/* core includes */
#include <kernel/processor.h>
#include <kernel/processor_pool.h>
#include <kernel/thread.h>
#include <kernel/irq.h>
#include <pic.h>
#include <timer.h>

namespace Kernel
{
	/**
	 * Lists all pending processor broadcasts
	 */
	class Processor_broadcast_list;

	Pic * pic();
	Timer * timer();
}

class Kernel::Processor_broadcast_list
:
	public Double_list<Processor_broadcast>
{
	public:

		/**
		 * Perform all pending broadcasts on the executing processor
		 */
		void for_each_perform_locally()
		{
			for_each([] (Processor_broadcast * const broadcast) {
				broadcast->_perform_locally();
			});
		}
};

namespace Kernel
{
	/**
	 * Return singleton processor-broadcast list
	 */
	Processor_broadcast_list * processor_broadcast_list()
	{
		static Processor_broadcast_list s;
		return &s;
	}
}


/**********************
 ** Processor_client **
 **********************/

void Kernel::Processor_client::_interrupt(unsigned const processor_id)
{
	/* determine handling for specific interrupt */
	unsigned irq_id;
	Pic * const ic = pic();
	if (ic->take_request(irq_id))
	{
		/* check wether the interrupt is a processor-scheduling timeout */
		if (timer()->interrupt_id(processor_id) == irq_id) {

			_processor->scheduler()->yield_occupation();
			timer()->clear_interrupt(processor_id);

		/* check wether the interrupt is our inter-processor interrupt */
		} else if (ic->is_ip_interrupt(irq_id, processor_id)) {

			processor_broadcast_list()->for_each_perform_locally();
			_processor->ip_interrupt_handled();

		/* after all it must be a user interrupt */
		} else {

			/* try to inform the user interrupt-handler */
			Irq::occurred(irq_id);
		}
	}
	/* end interrupt request at controller */
	ic->finish_request();
}


void Kernel::Processor_client::_schedule() { _processor->schedule(this); }


/***************
 ** Processor **
 ***************/

void Kernel::Processor::schedule(Processor_client * const client)
{
	if (_id != executing_id()) {

		/*
		 * Remote add client and let target processor notice it if necessary
		 *
		 * The interrupt controller might provide redundant submission of
		 * inter-processor interrupts. Thus its possible that once the targeted
		 * processor is able to grab the kernel lock, multiple remote updates
		 * occured and consequently the processor traps multiple times for the
		 * sole purpose of recognizing the result of the accumulative changes.
		 * Hence, we omit further interrupts if there is one pending already.
		 * Additionailly we omit the interrupt if the insertion doesn't
		 * rescind the current scheduling choice of the processor.
		 */
		if (_scheduler.insert_and_check(client)) { trigger_ip_interrupt(); }

	} else {

		/* add client locally */
		_scheduler.insert(client);
	}
}


void Kernel::Processor::trigger_ip_interrupt()
{
	if (!_ip_interrupt_pending) {
		pic()->trigger_ip_interrupt(_id);
		_ip_interrupt_pending = true;
	}
}


void Kernel::Processor_client::_unschedule()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->remove(this);
}


void Kernel::Processor_client::_yield()
{
	assert(_processor->id() == Processor::executing_id());
	_processor->scheduler()->yield_occupation();
}


/*************************
 ** Processor_broadcast **
 *************************/

void Kernel::Processor_broadcast::_perform_locally()
{
	/* perform broadcast method locally and get pending bit */
	unsigned const processor_id = Processor::executing_id();
	if (!_pending[processor_id]) { return; }
	_processor_broadcast_method();
	_pending[processor_id] = false;

	/* check wether there are still processors pending */
	unsigned i = 0;
	for (; i < PROCESSORS && !_pending[i]; i++) { }
	if (i < PROCESSORS) { return; }

	/* as no processors pending anymore, end the processor broadcast */
	processor_broadcast_list()->remove(this);
	_processor_broadcast_unblocks();
}


void Kernel::Processor_broadcast::_perform()
{
	/* perform locally and leave it at that if in uniprocessor mode */
	_processor_broadcast_method();
	if (PROCESSORS == 1) { return; }

	/* inform other processors and block until they are done */
	_processor_broadcast_blocks();
	processor_broadcast_list()->insert_tail(this);
	unsigned const processor_id = Processor::executing_id();
	for (unsigned i = 0; i < PROCESSORS; i++) {
		if (i == processor_id) { continue; }
		_pending[i] = true;
		processor_pool()->processor(i)->trigger_ip_interrupt();
	}
}
