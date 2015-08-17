/*
 * \brief  Multiprocessing-related CPU implementations
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>

/**
 * Multiprocessor affinity register
 */
struct Mpidr : Genode::Register<32> {
	struct Aff_0 : Bitfield<0, 8> { }; };


static Mpidr::access_t mpidr()
{
	Mpidr::access_t v;
	asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v) ::);
	return v;
}


unsigned Kernel::cpu_primary_id() { return 0; }

unsigned Kernel::cpu_executing_id() { return Mpidr::Aff_0::get(mpidr()); }
