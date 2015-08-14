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

bool volatile board_start_init_mp_async_sec;

/**
 * Diagnostic Register
 */
struct Diagr : Register<32>
{
	struct Bit_4  : Bitfield<4, 1> { };
	struct Bit_11 : Bitfield<11, 1> { };

	static access_t read()
	{
		access_t v;
		asm volatile ("mrc p15, 0, %0, c15, c0, 1" : "=r" (v) :: );
		return v;
	}

	static void write(access_t const v) {
		asm volatile ("mcr p15, 0, %0, c15, c0, 1" :: "r" (v) : ); }
};


class Scu : public Mmio
{
	friend Unmanaged_singleton_constructor;

	private:

		struct Cr : Register<0x0, 32> { struct Enable : Bitfield<0, 1> { }; };
		struct Dcr : Register<0x30, 32> { struct Bit_0 : Bitfield<0, 1> { }; };

		struct Iassr : Register<0xc, 32>
		{
			struct Cpu0_way : Bitfield<0, 4> { };
			struct Cpu1_way : Bitfield<4, 4> { };
			struct Cpu2_way : Bitfield<8, 4> { };
			struct Cpu3_way : Bitfield<12, 4> { };
		};

		Scu() : Mmio(Board::CORTEX_A9_SCU_MMIO_BASE) { }

	public:


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

		void enable()
		{
			if (Board::ARM_ERRATUM_764369()) { write<Dcr::Bit_0>(1); }
			write<Cr>(Cr::Enable::bits(1));
		}

		static Scu * singleton() {
			return unmanaged_singleton<Scu>(); }
};


void board_init_mp_async_sec(Kernel::Pd * const core_pd)
{
	while (!board_start_init_mp_async_sec) { }
	Cpu::dcisw_for_all_set_way();
	Cpu::iciallu();
	Cpu::flush_tlb_raw();
	Cpu::init_virt_kernel(core_pd);
	cpu_actlr_smp_bit_raise();
}


void board_init_mp_async_prim(Kernel::Pd * const core_pd)
{
	l2_cache()->disable();
	Cpu::dcisw_for_all_set_way();
	Cpu::iciallu();
	scu()->inval_dupl_tags_all_cpus();
	l2_cache()->invalidate();
	scu()->enable();
	Cpu::flush_tlb();
	Cpu::init_virt_kernel(core_pd);
	l2_cache()->enable();
	cpu_actlr_smp_bit_raise();
	board_start_init_mp_async_sec = true;
	Cpu::flush_data_caches();
}


void board_init_diagr()
{
	constexpr bool diagr_4 = Board::ARM_ERRATUM_742230;
	constexpr bool diagr_11 = Board::ARM_ERRATUM_751472;
	constexpr bool diagr = diagr_4 || diagr_11;
	if (!diagr) { return; }
	Diagr::access_t v = Diagr::read();
	if (diagr_4)  { Diagr::Bit_4::set(v, 1); }
	if (diagr_11) { Diagr::Bit_11::set(v, 1); }
	Diagr::write(v);
}

void board_init_mp_async(bool const primary, Kernel::Pd * const core_pd)
{
	board_init_diagr();
	Cpu::init_psr(1);
	Cpu::init_sctlr(0);
	if (primary) { board_init_mp_async_prim(core_pd); }
	else { board_init_mp_async_sec(core_pd); }
	Cpu::init_advanced_fp_simd();
}


void board_init_mp_sync(unsigned const cpu)
{
	init_trustzone(Kernel::pic());
	Kernel::perf_counter()->enable();
	Kernel::pic()->init_cpu_local();
	Kernel::pic()->unmask(Kernel::Timer::interrupt_id(cpu), cpu);
}
