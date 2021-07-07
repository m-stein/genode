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

/* core includes */
#include <fpu.h>

namespace Kernel { struct Thread_fault; }


namespace Board { class Address_space_id_allocator; }


namespace Genode {

	class Cpu;
	using sizet_arithm_t = __uint128_t;
}


class Genode::Cpu : private Hw::X86_64_cpu
{
	private:

		/**
		 * Task State Segment (TSS)
		 *
		 * See Intel SDM Vol. 3A, section 7.7
		 */
		struct alignas(8) Tss
		{
			uint32_t reserved0;
			uint64_t rsp[3];       /* pl0-3 stack pointer */
			uint64_t reserved1;
			uint64_t ist[7];       /* irq stack pointer   */
			uint64_t reserved2;

		}  __attribute__((packed));

		/**
		 * Global Descriptor Table (GDT)
		 *
		 * See Intel SDM Vol. 3A, section 3.5.1
		 */
		struct alignas(8) Gdt
		{
			uint64_t null_desc         { 0 };
			uint64_t sys_cs_64bit_desc { 0x20980000000000 };
			uint64_t sys_ds_64bit_desc { 0x20930000000000 };
			uint64_t usr_cs_64bit_desc { 0x20f80000000000 };
			uint64_t usr_ds_64bit_desc { 0x20f30000000000 };
			uint64_t tss_desc[2];

		} __attribute__((packed));

		/**
		 * Extends basic CPU state by members relevant for 'base-hw' only
		 */
		struct Cpu_context_base
		{
			addr_t kernel_stack { 0 };
		};

		Tss _tss { };
		Gdt _gdt { };

	public:

		/*
		 * Memory management context
		 */
		struct Mmu_context
		{
			addr_t cr3;

			Mmu_context(addr_t                             page_table_base,
			            Board::Address_space_id_allocator &);
		};

		/**
		 * Extends basic CPU state by members relevant for 'base-hw' only
		 *
		 * Note that exception_vector.s depends on the order of
		 * Cpu_state and Cpu_context_base in the inheritance list.
		 */
		struct alignas(16) Context : Cpu_state, Cpu_context_base, Fpu_context
		{
			enum Eflags {
				EFLAGS_IF_SET = 1 << 9,
				EFLAGS_IOPL_3 = 3 << 12,
			};

			Context(bool privileged);

		} __attribute__((packed));

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id();

		/**
		 * Switch to new context
		 *
		 * \param context  next CPU context
		 */
		void switch_to(Context & context, Mmu_context &mmu_context);

		/**
		 * Read out the page fault parameters from a given CPU state
		 */
		static void mmu_fault(Context & regs, Kernel::Thread_fault & fault);

		/**
		 * Invalidate the whole TLB
		 */
		static void invalidate_tlb() {
			Genode::Cpu::Cr3::write(Genode::Cpu::Cr3::read()); }

		/**
		 * Zero-out RAM region in prep. for being mapped to an address space
		 */
		static void clear_memory_region(addr_t const addr,
		                                size_t const size,
		                                bool changed_cache_properties);

		/**
		 * Constructor
		 */
		Cpu();

		/**
		 * Initialization parts that must be done separately from construction
		 */
		void finish_initialization();
};

#endif /* _CORE__SPEC__X86_64__CPU_H_ */
