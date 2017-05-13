/*
 * \brief  Kernel entrypoint for non-SMP systems
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-10-20
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>

using namespace Kernel;


extern "C" void kernel()
{
	Cpu &cpu = cpu_pool().current_cpu();
	cpu.schedule().proceed(cpu.id());
}


void Cpu::Ipi::occurred() { }


void Cpu::Ipi::trigger(unsigned const cpu_id) { }


Cpu::Ipi::Ipi(Irq::Pool &p) : Irq(Pic::IPI, p) { }
