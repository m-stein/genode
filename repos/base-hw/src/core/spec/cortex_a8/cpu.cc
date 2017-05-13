/*
 * \brief  CPU driver for core Arm Cortex A8 specific implementation
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2015-12-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <cpu.h>
#include <kernel/kernel.h>
#include <kernel/cpu.h>

using namespace Kernel;


void Genode::Cpu::translation_added(addr_t const addr,
                                    size_t const size)
{
	/*
	 * The Cortex-A8 CPU can't use the L1 cache on page-table
	 * walks. Therefore, as the page-tables lie in write-back cacheable
	 * memory we've to clean the corresponding cache-lines even when a
	 * page table entry is added. We only do this as core as the kernel
	 * adds translations solely before MMU and caches are enabled.
	 */
	if (is_user()) {
		update_data_region(addr, size);
	} else {
		cpu_pool().current_cpu().clean_invalidate_data_cache();
	}
}
