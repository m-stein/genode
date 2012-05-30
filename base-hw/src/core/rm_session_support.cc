/*
 * \brief  RM- and pager implementations specific for base-hw and core
 * \author Martin Stein
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>

/* Core includes */
#include <rm_session_component.h>
#include <platform.h>
#include <platform_thread.h>
#include <assert.h>
#include <software_tlb.h>

using namespace Genode;


/***************
 ** Rm_client **
 ***************/


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	/* Get software TLB of the thread that we serve */
	Platform_thread * const pt = Kernel::get_thread(badge());
	assert(pt);
	Software_tlb * const tlb = pt->software_tlb();
	assert(tlb);

	/* Update all translation caches */
	tlb->remove_region(virt_base, size);
	Kernel::update_pd(pt->pd_id());

	/* Try to regain administrative memory that has been freed by unmap */
	size_t s;
	void * base;
	while (tlb->regain_memory(base, s)) platform()->ram_alloc()->free(base, s);
}


/***************
 ** Ipc_pager **
 ***************/


void Ipc_pager::resolve_and_wait_for_fault()
{
	/* Valid mapping? */
	assert(_mapping.valid());

	/* Do we need extra space to resolve pagefault? */
	Software_tlb * const tlb = _pagefault.software_tlb;
	unsigned sl2 = tlb->insert_translation(_mapping.virt_address,
	               _mapping.phys_address, _mapping.size_log2,
	               1, _mapping.writable, 1, 0);
	if (sl2)
	{
		/* Try to get some natural aligned space */
		void * space;
		assert(platform()->ram_alloc()->alloc_aligned(1<<sl2, &space, sl2));

		/* Try to translate again with extra space */
		sl2 = tlb->insert_translation(_mapping.virt_address,
		                              _mapping.phys_address,
		                              _mapping.size_log2,
		                              1, _mapping.writable, 1, 0, space);
		assert(!sl2);
	}

	/* Try to wake up faulter */
	assert(!Kernel::resume_thread(_pagefault.thread_id));

	/* Wait for next page fault */
	wait_for_fault();
}

