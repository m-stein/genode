/*
 * \brief   A timer manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/cpu.h>
#include <kernel/timer.h>
#include <kernel/configuration.h>
#include <hw/assert.h>

using namespace Kernel;


void Timer::Irq::occurred() { _cpu.scheduler().timeout(); }


Timer::Irq::Irq(unsigned id, Cpu &cpu)
:
	Kernel::Irq(id, cpu.irq_pool()), _cpu(cpu)
{ }
