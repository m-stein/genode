/*
 * \brief   Kernel backend for virtual machines
 * \author  Martin Stein
 * \date    2013-10-30
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__VM_H_
#define _KERNEL__VM_H_

/* Genode includes */
#include <cpu/cpu_state.h>

/* core includes */
#include <kernel/kernel.h>
#include <kernel/pd.h>
#include <kernel/signal_receiver.h>

namespace Kernel
{
	/**
	 * Kernel backend for a virtual machine
	 */
	class Vm;

	class Vm_ids : public Id_allocator<MAX_VMS> { };
	typedef Object_pool<Vm> Vm_pool;

	Vm_ids  * vm_ids();
	Vm_pool * vm_pool();
}

class Kernel::Vm : public Object<Vm, MAX_VMS, Vm_ids, vm_ids, vm_pool>,
                   public Cpu_job
{
	private:

		struct Vm_state : Genode::Cpu_state_modes
		{
			Genode::addr_t dfar;
		};

		Vm_state       * const _state;
		Signal_context * const _context;

	public:

		/**
		 * Constructor
		 *
		 * \param state    initial CPU state
		 * \param context  signal for VM exceptions other than interrupts
		 */
		Vm(void * const state, Signal_context * const context)
		:
			Cpu_job(Cpu_priority::min), _state((Vm_state * const)state),
			_context(context)
		{ Cpu_job::affinity(processor_pool()->primary_processor()); }


		/****************
		 ** Vm_session **
		 ****************/

		void run()   { Cpu_job::_schedule(); }

		void pause() { Cpu_job::_unschedule(); }


		/*************
		 ** Cpu_job **
		 *************/

		void exception(unsigned const processor_id)
		{
			switch(_state->cpu_exception) {
			case Genode::Cpu_state::INTERRUPT_REQUEST:
			case Genode::Cpu_state::FAST_INTERRUPT_REQUEST:
				_interrupt(processor_id);
				return;
			case Genode::Cpu_state::DATA_ABORT:
				_state->dfar = Processor::Dfar::read();
			default:
				Cpu_job::_unschedule();
				_context->submit(1);
			}
		}

		void proceed(unsigned const processor_id)
		{
			mtc()->continue_vm(_state, processor_id);
		}
};

#endif /* _KERNEL__VM_H_ */
