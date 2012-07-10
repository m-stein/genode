/*
 * \brief  Raw CPU state specific for Cortex A9
 * \author Martin Stein
 * \date   2012-06-12
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CORTEX_A9__BASE__CPU_CONTEXT_H_
#define _INCLUDE__CORTEX_A9__BASE__CPU_CONTEXT_H_

/* Genode includes */
#include <drivers/cpu/cortex_a9/core.h>
#include <base/native_types.h>

namespace Genode
{
	struct Cpu_context : Cortex_a9::Context
	{
		void fetch(Native_thread_id const thread_id)
		{
			r0 = Kernel::read_register(thread_id, 0);
			r1 = Kernel::read_register(thread_id, 1);
			r2 = Kernel::read_register(thread_id, 2);
			r3 = Kernel::read_register(thread_id, 3);
			r4 = Kernel::read_register(thread_id, 4);
			r5 = Kernel::read_register(thread_id, 5);
			r6 = Kernel::read_register(thread_id, 6);
			r7 = Kernel::read_register(thread_id, 7);
			r8 = Kernel::read_register(thread_id, 8);
			r9 = Kernel::read_register(thread_id, 9);
			r10 = Kernel::read_register(thread_id, 10);
			r11 = Kernel::read_register(thread_id, 11);
			r12 = Kernel::read_register(thread_id, 12);
			r13 = Kernel::read_register(thread_id, 13);
			r14 = Kernel::read_register(thread_id, 14);
			r15 = Kernel::read_register(thread_id, 15);
		}

		void override(Native_thread_id const thread_id)
		{
			Kernel::write_register(thread_id, 0, r0);
			Kernel::write_register(thread_id, 1, r1);
			Kernel::write_register(thread_id, 2, r2);
			Kernel::write_register(thread_id, 3, r3);
			Kernel::write_register(thread_id, 4, r4);
			Kernel::write_register(thread_id, 5, r5);
			Kernel::write_register(thread_id, 6, r6);
			Kernel::write_register(thread_id, 7, r7);
			Kernel::write_register(thread_id, 8, r8);
			Kernel::write_register(thread_id, 9, r9);
			Kernel::write_register(thread_id, 10, r10);
			Kernel::write_register(thread_id, 11, r11);
			Kernel::write_register(thread_id, 12, r12);
			Kernel::write_register(thread_id, 13, r13);
			Kernel::write_register(thread_id, 14, r14);
			Kernel::write_register(thread_id, 15, r15);
		}
	};
}

#endif /* _INCLUDE__CORTEX_A9__BASE__CPU_CONTEXT_H_ */

