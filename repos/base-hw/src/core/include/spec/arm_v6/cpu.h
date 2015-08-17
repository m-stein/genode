/*
 * \brief  CPU driver for core
 * \author Norman Feske
 * \author Martin stein
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_H_
#define _CPU_H_

/* core includes */
#include <spec/arm/cpu_support.h>
#include <assert.h>
#include <board.h>

namespace Genode
{
	/**
	 * Part of CPU state that is not switched on every mode transition
	 */
	class Cpu_lazy_state { };

	/**
	 * CPU driver for core
	 */
	class Cpu;
}

namespace Kernel {
	using Genode::Cpu_lazy_state;

	class Pd;
}

class Genode::Cpu : public Arm
{
	public:

		/**
		 * Cache type register
		 */
		struct Ctr : Arm::Ctr
		{
			struct P : Bitfield<23, 1> { }; /* page mapping restriction on */
		};

		/**
		 * If page descriptor bits [13:12] are restricted
		 */
		static bool restricted_page_mappings() {
			return Ctr::P::get(Ctr::read()); }

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void init_phys_kernel();

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param pd  kernel's pd object
		 */
		static void init_virt_kernel(Kernel::Pd* pd);

		/**
		 * Ensure that TLB insertions get applied
		 */
		static void tlb_insertions() { flush_tlb(); }

		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Cpu_lazy_state *) { return false; }

		/**
		 * Post processing after a translation was added to a translation table
		 *
		 * \param addr  virtual address of the translation
		 * \param size  size of the translation
		 */
		static void translation_added(addr_t const addr, size_t const size)
		{
			/*
			 * The Cortex-A8 CPU can't use the L1 cache on page-table
			 * walks. Therefore, as the page-tables lie in write-back cacheable
			 * memory we've to clean the corresponding cache-lines even when a
			 * page table entry is added. We only do this as core as the kernel
			 * adds translations solely before MMU and caches are enabled.
			 */
			if (is_user()) Kernel::update_data_region(addr, size);
			else flush_data_caches();
		}

		/*************
		 ** Dummies **
		 *************/

		static void prepare_proceeding(Cpu_lazy_state *, Cpu_lazy_state *) { }
		static void wait_for_interrupt() { /* FIXME */ }
		static void data_synchronization_barrier() { /* FIXME */ }
		static void invalidate_control_flow_predictions() { /* FIXME */ }
};

#endif /* _CPU_H_ */
