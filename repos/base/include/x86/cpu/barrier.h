/*
 * \brief  Implementation of various barriers
 * \author Martin Stein
 * \date   2014-11-12
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__X86__CPU__BARRIER_H_
#define _INCLUDE__X86__CPU__BARRIER_H_

namespace Genode
{
	/**
	 * Compiler memory barrier
	 */
	inline void cc_memory_barrier() { asm volatile ("" ::: "memory"); }


	/**
	 * Barrier issued after lock acquisition before accessing guarded resource
	 */
	inline void lock_acquire_barrier()
	{
		/*
		 * The compiler memory-barrier of the preceding 'cmpxchg' prevents the
		 * compiler from changing the program order of memory accesses in such
		 * a way that accesses to the guarded resource get outside the guarded
		 * stage. This is sufficient as the architecture ensures to not
		 * reorder writes with older reads, writes to memory with other writes
		 * (except in cases that are not relevant for our locks), or
		 * read/write instructions with I/O instructions, locked instructions,
		 * and serializing instructions.
		 */
	}


	/**
	 * Barrier issued after accessing guarded resource before lock release
	 */
	inline void lock_release_barrier()
	{
		/*
		 * The compiler memory-barrier ensures the correct program order of
		 * memory accesses as explained in 'lock_acquire_barrier'. This is
		 * sufficient as also explained in 'lock_acquire_barrier'.
		 */
		cc_memory_barrier();
	}
}

#endif /* _INCLUDE__X86__CPU__BARRIER_H_ */
