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

#ifndef _INCLUDE__ARM_V7__CPU__BARRIER_H_
#define _INCLUDE__ARM_V7__CPU__BARRIER_H_

namespace Genode
{
	/**
	 * Compiler memory barrier
	 */
	inline void cc_memory_barrier() { asm volatile ("" ::: "memory"); }


	/**
	 * Architectural memory barrier
	 */
	inline void arch_memory_barrier() { asm volatile ("dmb"); }


	/**
	 * Barrier issued after lock acquisition before accessing guarded resource
	 */
	inline void lock_acquire_barrier()
	{
		/*
		 * The compiler memory-barrier of the preceding 'cmpxchg' prevents the
		 * compiler from changing the program order of memory accesses in such
		 * a way that accesses to the guarded resource get outside the guarded
		 * stage. However, the architectural memory model allows not only that
		 * memory accesses take local effect in another order as their program
		 * order but also that different observers (components that can access
		 * memory like data-busses, TLBs and branch predictors) observe these
		 * effects each in another order. Thus, a correct program order isn't
		 * sufficient for a correct observation order. An additional
		 * architectural memory-barrier is needed to ensure that all observers
		 * observe our lock acquire before they observe any of our accesses to
		 * the guarded resource.
		 */
		arch_memory_barrier();
	}


	/**
	 * Barrier issued after accessing guarded resource before lock release
	 */
	inline void lock_release_barrier()
	{
		/*
		 * The compiler memory-barrier ensures the correct program order of
		 * memory accesses as explained in 'lock_acquire_barrier'. However,
		 * according to the architectural reordering, also explained in
		 * 'lock_acquire_barrier', a correct program order isn't sufficient
		 * for a correct observation order. Thus an additional architectural
		 * memory-barrier is needed to ensure that none of the observers
		 * observes any of our accesses to the guarded resource after he
		 * observed our lock release.
		 */
		arch_memory_barrier();
		cc_memory_barrier();
	}
}

 #endif /* _INCLUDE__ARM_V7__CPU__BARRIER_H_ */
