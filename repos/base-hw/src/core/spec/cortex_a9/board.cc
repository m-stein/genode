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

/* Genode includes */
#include <util/mmio.h>

/* base includes */
#include <unmanaged_singleton.h>

/* core includes */
#include <board.h>
#include <trustzone.h>
#include <kernel/perf_counter.h>
#include <cpu.h>
#include <pic.h>
#include <timer.h>

using namespace Genode;

namespace Kernel
{
	/**
	 * Return interrupt-controller singleton
	 */
	Pic * pic() { return unmanaged_singleton<Pic>(); }
}

unsigned volatile start_init_mp_async_secnd;

class Scu : public Mmio
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

		Scu(addr_t const base) : Mmio(base) {
			if (!ENABLE_SCU) write<Cr>(Cr::Enable::bits(0)); }

		void inval_dupl_tags_all_cpus()
		{
			if (!ENABLE_SCU) { return; }
			Iassr::access_t iassr = 0;
			for (Iassr::access_t way = 0; way <= Iassr::Cpu0_way::mask(); way++) {
				Iassr::Cpu0_way::set(iassr, way);
				Iassr::Cpu1_way::set(iassr, way);
				Iassr::Cpu2_way::set(iassr, way);
				Iassr::Cpu3_way::set(iassr, way);
				write<Iassr>(iassr);
			}
		}

		void enable()
		{
			if (!ENABLE_SCU) { return; }
			/* enable SCU */
			write<Cr>(Cr::Enable::bits(1)); }
};


static Scu * scu() {
	return unmanaged_singleton<Scu>(Board::CORTEX_A9_SCU_MMIO_BASE); }



void init_mp_async_secnd(addr_t const core_tt, unsigned const core_id)
{
	while (!start_init_mp_async_secnd) { }
	Cpu::dcisw_for_all_set_way();
	Cpu::iciallu();
	Cpu::flush_tlb_raw();
	Cpu::init_virt_kernel(core_tt, core_id);
	Board::raise_actlr_smp_bit();
}


void init_mp_async_prim(addr_t const core_tt, unsigned const core_id)
{
	l2_cache()->disable();
	Cpu::dcisw_for_all_set_way();
	Cpu::iciallu();
	scu()->inval_dupl_tags_all_cpus();
	l2_cache()->invalidate();
	scu()->enable();
	Cpu::flush_tlb();
	Cpu::init_virt_kernel(core_tt, core_id);
	l2_cache()->enable();
	Board::raise_actlr_smp_bit();
	start_init_mp_async_secnd = 1;
	Cpu::flush_data_caches();
}


void Board::init_mp_async(bool const primary, addr_t const core_tt,
                          unsigned const core_id)
{
	Cpu::init_psr(1);
	Cpu::init_sctlr(0);
	if (primary) { init_mp_async_prim(core_tt, core_id); }
	else { init_mp_async_secnd(core_tt, core_id); }
	Cpu::init_advanced_fp_simd();
}


void Board::init_mp_sync(unsigned const cpu)
{
	init_trustzone(Kernel::pic());
	Kernel::perf_counter()->enable();
	Kernel::pic()->init_cpu_local();
	Kernel::pic()->unmask(Kernel::Timer::interrupt_id(cpu), cpu);
}
