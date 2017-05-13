/*
 * \brief   ARM non-SMP specific kernel thread implementations
 * \author  Stefan Kalkowski
 * \date    2015-12-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/pd.h>
#include <kernel/thread.h>

void Kernel::Thread::_call_update_pd()
{
	Pd * const pd = (Pd *) user_arg_1();
	Cpu &cpu = cpu_pool().current_cpu();
	cpu.invalidate_instr_cache();
	cpu.clean_invalidate_data_cache();
	Cpu::Tlbiasid::write(pd->asid); /* flush TLB by ASID */
}
