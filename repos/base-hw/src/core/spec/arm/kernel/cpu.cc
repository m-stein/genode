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

/* core includes */
#include <kernel/cpu.h>
#include <kernel/kernel.h>

using namespace Kernel;


void Cpu_context::_init(size_t const stack_size, addr_t const table)
{
	r12 = stack_size;
	cpu_exception = Genode::Cpu::Ttbr0::init(table);
}


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::min, 0)
{
	Cpu_job::cpu(cpu);
	cpu_exception = RESET;
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->id());
}


void Cpu_idle::exception(unsigned const cpu)
{
	switch (cpu_exception) {
	case INTERRUPT_REQUEST:      _interrupt(cpu); return;
	case FAST_INTERRUPT_REQUEST: _interrupt(cpu); return;
	case RESET:                                   return;
	default: assert(0); }
}
