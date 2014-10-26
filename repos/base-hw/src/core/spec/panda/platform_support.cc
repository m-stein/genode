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

using namespace Genode;

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


Cpu::User_context::User_context() { cpsr = Psr::init_user(); }


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

bool volatile board_init_mp_primary_done;

void Board::secondary_cpus_ip(void * const ip)
{
	using Kernel::Cpu;
	wugen()->init_cpu_1(ip);
	board_init_mp_primary_done = 0;
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


void cpu_inval_l1_dcache()
{
	asm volatile (
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_0
		WRITE_DCISW(r6)
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_1);
}

void board_init_mp_primary(addr_t tt_base, unsigned pd_id)
{
	using Kernel::Cpu;
	scu()->inval_dupl_tags_all_cpus();
	cpu_inval_l1_dcache();
	scu()->enable();
	Actlr::access_t actlr = Actlr::read();
	Actlr::Smp::set(actlr, 1);
	Actlr::write(actlr);
	Cpu::invalidate_instr_caches();
	Cpu::data_synchronization_barrier();
	Cpu::init_phys_kernel();
	Cpu::init_virt_kernel(tt_base, pd_id);
	board_init_mp_primary_done = 1;
	Cpu::flush_data_caches();
}

void board_init_mp_secondary(addr_t tt_base, unsigned pd_id)
{
	using Kernel::Cpu;
	while (!board_init_mp_primary_done) { }
	cpu_inval_l1_dcache();
	scu()->wait_till_enabled();
	Actlr::access_t actlr = Actlr::read();
	Actlr::Smp::set(actlr, 1);
	Actlr::write(actlr);
	Cpu::invalidate_instr_caches();
	Cpu::data_synchronization_barrier();
	Cpu::init_psr();
	Cpu::flush_tlb_raw();
	Cpu::finish_init_phys_kernel();
	Cpu::init_virt_kernel(tt_base, pd_id);
}

void board_init_mp(addr_t const pd_tt, unsigned const pd_id)
{
	using Kernel::Cpu;
	if (Cpu::executing_id() == Cpu::primary_id()) {
		board_init_mp_primary(pd_tt, pd_id); }
	else { board_init_mp_secondary(pd_tt, pd_id); }
}
