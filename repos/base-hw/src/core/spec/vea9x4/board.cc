/*
 * \brief   Board driver for core
 * \author  Martin Stein
 * \date    2014-12-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>
#include <pic.h>

/* base includes */
#include <unmanaged_singleton.h>

using namespace Genode;

extern unsigned volatile start_init_mp_async_secnd;

namespace Kernel { Pic * pic(); }

unsigned Cpu::executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }

unsigned Cpu::primary_id() { return 0; }


void Arm_v7::start_secondary_cpus(void * const ip)
{
	if (!(NR_OF_CPUS > 1)) { return; }
	Board::secondary_cpus_ip(ip);
	data_synchronization_barrier();
	Kernel::pic()->trigger_ip_interrupt_all_cpus();
}

namespace Genode
{
	L2_cache * l2_cache() {
		return unmanaged_singleton<L2_cache>(); }
}

class System_registers : public Mmio
{
	private:

		struct Flagsset : Register<0x30, 32> { };
		struct Flagsclr : Register<0x34, 32> { };

	public:

		System_registers() : Mmio(Board::SYSTEM_REGISTERS_MMIO_BASE) { }

		void flags(Flagsset::access_t const f)
		{
			write<Flagsclr>(~0);
			write<Flagsset>(f);
		}
};

System_registers * system_registers() {
	return unmanaged_singleton<System_registers>(); }

void Board::secondary_cpus_ip(void * const ip)
{
	system_registers()->flags((addr_t)ip);
	start_init_mp_async_secnd = 0;
	Cpu::flush_data_caches();
}

struct Actlr : Register<32>
{
	struct Smp : Bitfield<6, 1> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c1, c0, 1" : "=r" (v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile ("mcr p15, 0, %0, c1, c0, 1" :: "r" (v) : ); }
};

void Board::raise_actlr_smp_bit()
{
	Actlr::access_t actlr = Actlr::read();
	Actlr::Smp::set(actlr, 1);
	Actlr::write(actlr);
}
