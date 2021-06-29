/*
 * \brief  x86_64 CPU driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Martin stein
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__CPU_H_
#define _CORE__SPEC__X86_64__CPU_H_

/* Genode includes */
#include <util/register.h>
#include <kernel/interface_support.h>
#include <cpu/cpu_state.h>

#include <hw/spec/x86_64/cpu.h>

/* base includes */
#include <base/internal/align_at.h>
#include <base/internal/unmanaged_singleton.h>

/* core includes */
#include <fpu.h>

/* Spunky includes */
#include <kernel/ada_object.h>

namespace Kernel { struct Thread_fault; }


namespace Genode {

	class Cpu;
	using sizet_arithm_t = __uint128_t;
}


class Genode::Cpu : public Kernel::Ada_object<160>
{
	private:

		struct Cpu_context_base
		{
			addr_t not_used { 0 };
		};

	public:

		struct Mmu_context
		{
			addr_t not_used;

			Mmu_context(addr_t page_table_base);
		};

		struct alignas(16) Context : Cpu_state, Cpu_context_base, Fpu_context
		{
			Context(bool privileged);

		} __attribute__((packed));

		static unsigned executing_id();

		void switch_to(Context & context, Mmu_context &mmu_context);

		static void mmu_fault(Context & regs, Kernel::Thread_fault & fault);

		static void invalidate_tlb();

		static void clear_memory_region(addr_t const addr,
		                                size_t const size,
		                                bool changed_cache_properties);

		void arch_init();

		Cpu();
};

#endif /* _CORE__SPEC__X86_64__CPU_H_ */
