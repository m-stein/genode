/*
 * \brief   Kernel interface for inter-processor communication
 * \author  Stefan Kalkowski
 * \author  Martin Stein
 * \date    2018-11-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw Core includes */
#include <kernel/inter_processor_work.h>
#include <kernel/cpu.h>


void Kernel::Inter_processor_work::_execute_tlb_invalidation()
{
	Genode::Cpu::invalidate_tlb();
};
