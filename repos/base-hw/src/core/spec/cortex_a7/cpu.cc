/*
 * \brief   Cpu driver implementations specific to ARM Cortex A15
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>
#include <cpu.h>

void Genode::Arm_v7::enable_mmu_and_caches(Kernel::Pd & pd)
{
	Cpu::Mair0::write(Cpu::Mair0::init_virt_kernel());
	Cpu::Dacr::write(Cpu::Dacr::init_virt_kernel());
	Cpu::Ttbr0::write(Cpu::Ttbr0::init((Genode::addr_t)pd.translation_table(), pd.asid));
	Cpu::Ttbcr::write(Cpu::Ttbcr::init_virt_kernel());
	Cpu::Sctlr::enable_mmu_and_caches();
	invalidate_branch_predicts();
}

void Genode::Cpu::translation_added(Genode::addr_t const addr,
                                    Genode::size_t const size)
{
	using namespace Kernel;

	/*
	 * The Cortex-A8 CPU can't use the L1 cache on page-table
	 * walks. Therefore, as the page-tables lie in write-back cacheable
	 * memory we've to clean the corresponding cache-lines even when a
	 * page table entry is added. We only do this as core as the kernel
	 * adds translations solely before MMU and caches are enabled.
	 */
	if (is_user()) update_data_region(addr, size);
	else {
		Cpu * const cpu  = cpu_pool()->cpu(Cpu::executing_id());
		cpu->clean_invalidate_data_cache();
	}
}
