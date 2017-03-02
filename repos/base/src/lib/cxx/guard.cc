/*
 * \brief  Support for guarded variables
 * \author Christian Helmuth
 * \date   2009-11-18
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <base/registry.h>
#include <base/semaphore.h>
#include <cpu/atomic.h>


struct Block : Genode::Registry<Block>::Element
{
	Genode::Semaphore sem { 0 };
	Block(Genode::Registry<Block> &registry)
	: Genode::Registry<Block>::Element(registry, *this) { }
};


static Genode::Registry<Block> blocked;


namespace __cxxabiv1 
{

	/*
	 * A guarded variable can be in three states:
	 *
	 *   1) not initialized
	 *   2) in initialization (transient)
	 *   3) initialized
	 *
	 * The generic ABI uses the first byte of a 64-bit guard variable for state
	 * 1), 2) and 3). ARM-EABI uses the first byte of a 32-bit guard variable.
	 * Therefore we define '__guard' as a 32-bit type and use the least
	 * significant byte for 1) and 3) and the following byte for 2) and let the
	 * other threads spin until the guard is released by the thread in
	 * initialization.
	 */

	typedef int __guard;

	enum State { INIT_NONE = 0, INIT_DONE = 1, IN_INIT = 0x100, WAITERS = 0x200 };

	extern "C" int __cxa_guard_acquire(__guard *guard)
	{
		volatile char *initialized = (char *)guard;
		volatile int  *in_init     = (int *)guard;

		/* check for state 3) */
		if (*initialized == INIT_DONE) return 0;

		/* atomically check and set state 2) */
		if (!Genode::cmpxchg(in_init, INIT_NONE, IN_INIT)) {

			/* register current thread for blocking */
			Block block(blocked);

			/* tell guard thread that current thread needs a wakeup */
			while (!Genode::cmpxchg(in_init, *in_init, *in_init | WAITERS)) ;

			/* wait until state 3) is reached by guard thread */
			while (*initialized != INIT_DONE)
				block.sem.down();

			/* guard not acquired */
			return 0;
		}

		/*
		 * The guard was acquired. The caller is allowed to initialize the
		 * guarded variable and required to call __cxa_guard_release() to flag
		 * initialization completion (and unblock other guard applicants).
		 */
		return 1;
	}


	extern "C" void __cxa_guard_release(__guard *guard)
	{
		volatile int  *in_init     = (int *)guard;

		/* set state 3) */
		while (!Genode::cmpxchg(in_init, *in_init, *in_init | INIT_DONE)) ;

		/* check whether somebody blocked on this guard */
		if (!(*in_init & WAITERS))
			return;

		/* we had contention - wake all up */
		blocked.for_each([](Block &wake) { wake.sem.up(); });
	}


	extern "C" void __cxa_guard_abort(__guard *) {
		Genode::error(__func__, " called"); }
}
