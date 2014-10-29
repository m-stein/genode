/*
 * \brief   Platform implementations specific for base-hw and Panda A2
 * \author  Martin Stein
 * \date    2012-04-27
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
#include <unmanaged_singleton.h>
#include <kernel/cpu.h>
#include <trustzone.h>
#include <kernel/perf_counter.h>

using namespace Genode;

namespace Kernel
{
	/**
	 * Return interrupt-controller singleton
	 */
	Pic * pic() { return unmanaged_singleton<Pic>(); }
}

Native_region * Platform::_ram_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::RAM_0_BASE, Board::RAM_0_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		{ Board::MMIO_0_BASE, Board::MMIO_0_SIZE },
		{ Board::MMIO_1_BASE, Board::MMIO_1_SIZE },
		{ Board::DSS_MMIO_BASE, Board::DSS_MMIO_SIZE },
		{ Board::DISPC_MMIO_BASE, Board::DISPC_MMIO_SIZE },
		{ Board::HDMI_MMIO_BASE, Board::HDMI_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Native_region * Platform::_core_only_mmio_regions(unsigned const i)
{
	static Native_region _regions[] =
	{
		/* core timer and PIC */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE,
		  Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* core UART */
		{ Board::TL16C750_3_MMIO_BASE, Board::TL16C750_MMIO_SIZE },

		/* l2 cache controller */
		{ Board::PL310_MMIO_BASE, Board::PL310_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Cpu::User_context::User_context() { cpsr = Cpu::init_psr_value(0); }


static Board::Pl310 * l2_cache() {
	return unmanaged_singleton<Board::Pl310>(Board::PL310_MMIO_BASE); }


void Board::outer_cache_invalidate() { l2_cache()->invalidate(); }
void Board::outer_cache_flush()      { l2_cache()->flush();      }
void Board::prepare_kernel()         { l2_cache()->invalidate(); }

class Wugen : Mmio
{
	private:

		struct Aux_core_boot_0 : Register<0x800, 32> {
			struct Cpu1_status : Bitfield<2, 2> { }; };

		struct Aux_core_boot_1 : Register<0x804, 32> { };

	public:

		Wugen(addr_t const base) : Mmio(base) { }

		void init_cpu_1(void * const ip)
		{
			write<Aux_core_boot_1>((addr_t)ip);
			write<Aux_core_boot_0::Cpu1_status>(1);
		}
};

static Wugen * wugen() {
	return unmanaged_singleton<Wugen>(Board::CORTEX_A9_WUGEN_MMIO_BASE); }

unsigned volatile init_mp_prim_step;

void Board::secondary_cpus_ip(void * const ip)
{
	using Kernel::Cpu;
	wugen()->init_cpu_1(ip);
	init_mp_prim_step = 0;
	Cpu::flush_data_caches();
}

class Scu : Mmio
{
	private:

		struct Cr : Register<0x0, 32> { struct Enable : Bitfield<0, 1> { }; };

		struct Iassr : Register<0xc, 32>
		{
			struct Cpu0_way : Bitfield<0, 4> { };
			struct Cpu1_way : Bitfield<4, 4> { };
			struct Cpu2_way : Bitfield<8, 4> { };
			struct Cpu3_way : Bitfield<12, 4> { };
		};

	public:

		Scu(addr_t const base) : Mmio(base) { }

		void inval_dupl_tags_all_cpus()
		{
			Iassr::access_t iassr = 0;
			for (Iassr::access_t way = 0; way <= Iassr::Cpu0_way::mask(); way++) {
				Iassr::Cpu0_way::set(iassr, way);
				Iassr::Cpu1_way::set(iassr, way);
				Iassr::Cpu2_way::set(iassr, way);
				Iassr::Cpu3_way::set(iassr, way);
				write<Iassr>(iassr);
			}
		}

		void enable() { write<Cr>(Cr::Enable::bits(1)); }

		void wait_till_enabled() { while (!read<Cr::Enable>()) ; }
};

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

static Scu * scu() {
	return unmanaged_singleton<Scu>(Board::CORTEX_A9_SCU_MMIO_BASE); }

void init_mp_async_local()
{
	Cpu::init_psr(1);
	Cpu::init_sctlr(0);
}

void init_mp_async_step3()
{
	Actlr::access_t actlr = Actlr::read();
	Actlr::Smp::set(actlr, 1);
	Actlr::write(actlr);
	Cpu::iciallu();
}

void init_mp_async_prim(addr_t const core_tt, unsigned const core_id)
{
	init_mp_async_local();

	/* SMP L1 bring-up step 1 */
	scu()->inval_dupl_tags_all_cpus();
	Cpu::dcisw_for_all_set_way();
	init_mp_prim_step++;

	/* SMP L1 bring-up step 2 */
	scu()->enable();
	init_mp_prim_step++;

	/* SMP L1 bring-up step 3 */
	init_mp_async_step3();

	/* Cpu::init_phys_kernel(); */
	Board::prepare_kernel();
	Cpu::flush_tlb();
	Cpu::finish_init_phys_kernel();

	Cpu::init_virt_kernel(core_tt, core_id);
	init_mp_prim_step++;
	Cpu::flush_data_caches();
}

void init_mp_async_secnd(addr_t const core_tt, unsigned const core_id)
{
/*
 * Die Panda MPU ist r2p10 und die 3 steps Anleitung stammt von r3p0.
 * Für dieses Board muß die 7 steps Anleitung aus r2p0 genutzt werden.
 * Da wird auch der L2 cache initialisiert. r2p0 und r2p2 sind diesbezügl.
 * identisch.
 */
	init_mp_async_local();

	/* SMP L1 bring-up step 1 */
	while (init_mp_prim_step < 1) { }
	Cpu::dcisw_for_all_set_way();

	/* SMP L1 bring-up step 2 */
	while (init_mp_prim_step < 2) { }
	scu()->wait_till_enabled();

	/* SMP L1 bring-up step 3 */
	while (init_mp_prim_step < 3) { }
	init_mp_async_step3();
	Cpu::flush_tlb_raw();
	Cpu::finish_init_phys_kernel();
	Cpu::init_virt_kernel(core_tt, core_id);
}

void Board::init_mp_async(bool const primary, addr_t const core_tt,
                          unsigned const core_id)
{
	if (primary) { init_mp_async_prim(core_tt, core_id); }
	else { init_mp_async_secnd(core_tt, core_id); }
}

void Board::init_mp_sync(unsigned const cpu)
{
	init_trustzone(Kernel::pic());
	Kernel::perf_counter()->enable();
	Kernel::pic()->init_cpu_local();
	Kernel::pic()->unmask(Kernel::Timer::interrupt_id(cpu), cpu);
}
