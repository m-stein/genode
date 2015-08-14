/*
 * \brief  Singlethreaded minimalistic kernel
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-10-20
 *
 * This kernel is the only code except the mode transition PIC, that runs in
 * privileged CPU mode. It has two tasks. First it initializes the process
 * 'core', enriches it with the whole identically mapped address range,
 * joins and applies it, assigns one thread to it with a userdefined
 * entrypoint (the core main thread) and starts this thread in userland.
 * Afterwards it is called each time an exception occurs in userland to do
 * a minimum of appropriate exception handling. Thus it holds a CPU context
 * for itself as for any other thread. But due to the fact that it never
 * relies on prior kernel runs this context only holds some constant pointers
 * such as SP and IP.
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/lock.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>
#include <kernel/test.h>
#include <platform_pd.h>
#include <timer.h>
#include <pic.h>
#include <platform_thread.h>

/* base includes */
#include <unmanaged_singleton.h>
#include <base/native_types.h>

using namespace Kernel;

extern void * _start_secondary_cpus;

static_assert(sizeof(Genode::sizet_arithm_t) >= 2 * sizeof(size_t),
	"Bad result type for size_t arithmetics.");

Lock & Kernel::data_lock() { return *unmanaged_singleton<Kernel::Lock>(); }


Pd * Kernel::core_pd() {
	return unmanaged_singleton<Genode::Core_platform_pd>()->kernel_pd(); }


Pic * Kernel::pic() { return unmanaged_singleton<Pic>(); }

unsigned volatile kernel_init_mp_done;

/**
 * Kernel initialization till the activation of secondary CPUs
 */
extern "C" void kernel_init_up()
{
	/*
	 * As atomic operations are broken in physical mode on some platforms
	 * we must avoid the use of 'cmpxchg' by now (includes not using any
	 * local static objects.
	 */

	/* calculate in advance as needed later when data writes aren't allowed */
	core_pd();

	/* initialize all CPU objects */
	cpu_pool();

	/* initialize PIC */
	pic();

	/* go multiprocessor mode */
	kernel_init_mp_done = 0;
	cpu_start_secondary(&_start_secondary_cpus);
}


/**
 * Kernel initialization as primary CPU after the activation of secondary CPUs
 */
void kernel_init_mp_primary()
{
	Core_thread::singleton();
	Genode::printf("kernel initialized\n");
	test();
}


/**
 * Kernel initialization after the activation of secondary CPUs
 */
extern "C" void kernel_init_mp()
{
	unsigned const cpu = cpu_executing_id();
	bool const primary = cpu_primary_id() == cpu;
	board_init_mp_async(primary, core_pd());
	data_lock().lock();
	board_init_mp_sync(cpu);
	if (primary) { kernel_init_mp_primary(); }
	kernel_init_mp_done++;
	board_init_mp_done();
	data_lock().unlock();
	while (kernel_init_mp_done < NR_OF_CPUS) { }
}


extern "C" void kernel()
{
	data_lock().lock();
	cpu_pool()->cpu(cpu_executing_id())->exception();
}
