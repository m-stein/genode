/*
 * \brief   Specific implementations of cores platform representation
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <processor_driver.h>
#include <pic.h>
#include <unmanaged_singleton.h>

using namespace Genode;


/***************
 ** Utilities **
 ***************/

namespace Omap
{
	/**
	 * General system-controls of the board domain called core
	 */
	class Sysctrl_general_core;
}

namespace Omap4430
{
	/**
	 * General system-controls of the board domain called core
	 */
	class Sysctrl_general_core;

	/**
	 * Return singleton of Sysctrl_general_core
	 */
	static Sysctrl_general_core * sysctrl_general_core()
	{
		return unmanaged_singleton<Sysctrl_general_core>();
	}
}

namespace Omap4460
{
	/**
	 * General system-controls of the board domain called core
	 */
	class Sysctrl_general_core;

	/**
	 * Return singleton of Sysctrl_general_core
	 */
	static Sysctrl_general_core * sysctrl_general_core()
	{
		return unmanaged_singleton<Sysctrl_general_core>();
	}
}

/**
 * Power and reset manager
 */
class Prm : public Mmio
{
	private:

		/********************
		 ** MMIO structure **
		 ********************/

		struct Cm_sys_clksel : Register<0x110, 32>
		{
			struct Sys_clksel : Bitfield<0, 3> { };
		};

	public:

		/**
		 * Constructor
		 */
		Prm() : Mmio(Board::PRM_MMIO_BASE) { }

		/**
		 * Return system clock
		 */
		unsigned sys_clk()
		{
			switch (read<Cm_sys_clksel::Sys_clksel>()) {
			case 7: return 38400000;
			default:
				PWRN("unkown system clock");
				return 0;
			}
		}
};

/**
 * Clock manager 1 region A
 */
class Cm1_region_a : public Mmio
{
	private:

		/********************
		 ** MMIO structure **
		 ********************/

		struct Clkmode_dpll_mpu : Register <0x160, 32>
		{
			struct Dpll_en : Bitfield<0, 3>
			{
				static constexpr access_t fast_relock_mode() { return 6; }
				static constexpr access_t lock_mode() { return 7; }
			};
		};

		struct Idlest_dpll_mpu : Register <0x164, 32>
		{
			struct St_dpll_clk : Bitfield<0, 1>
			{
				static constexpr access_t bypassed() { return 0; }
				static constexpr access_t locked() { return 1; }
			};
		};

		struct Clksel_dpll_mpu : Register <0x16c, 32>
		{
			struct Dpll_div  : Bitfield<0, 7> { };
			struct Dpll_mult : Bitfield<8, 11> { };
		};

		struct Div_m2_dpll_mpu : Register <0x170, 32>
		{
			struct Dpll_clkout_div : Bitfield<0, 5> { };
		};

	public:

		/**
		 * Constructor
		 */
		Cm1_region_a() : Mmio(Board_base::CM1_REGION_A_MMIO_BASE) { }

		/**
		 * Configure MPU DPLL according for a given setup
		 *
		 * \param mult        clock multiplier
		 * \param div         clock divider - 1
		 * \param clkout_div  post clock divider
		 */
		void setup_dpll_mpu(unsigned const mult, unsigned const div,
		                    unsigned const clkout_div)
		{
			typedef Clkmode_dpll_mpu        Clkmode;
			typedef Clkmode::Dpll_en        Dpll_en;
			typedef Idlest_dpll_mpu         Idlest;
			typedef Idlest::St_dpll_clk     St_dpll_clk;
			typedef Clksel_dpll_mpu         Clksel;
			typedef Clksel::Dpll_div        Dpll_div;
			typedef Clksel::Dpll_mult       Dpll_mult;
			typedef Div_m2_dpll_mpu         Div_m2;
			typedef Div_m2::Dpll_clkout_div Dpll_clkout_div;

			/* remember DPLL config */
			Clksel::access_t clksel = read<Clksel>();

			/* bypass DPLL */
			write<Dpll_en>(Dpll_en::fast_relock_mode());
			while (read<St_dpll_clk>() != St_dpll_clk::bypassed()) { }

			/* write DPLL config with new multiplier and divider */
			Dpll_mult::set(clksel, mult);
			Dpll_div::set(clksel, div);
			write<Clksel>(clksel);

			/* lock DPLL and write DPLL post-divider */
			write<Dpll_en>(Dpll_en::lock_mode());
			write<Dpll_clkout_div>(clkout_div);
			while (read<St_dpll_clk>() != St_dpll_clk::locked()) { }
		}
};

class Omap::Sysctrl_general_core : public Mmio
{
	protected:

		/********************
		 ** MMIO structure **
		 ********************/

		struct Std_fuse_prod_id_1 : Register <0x218, 32>
		{
			struct Silicon_type : Bitfield<16, 2> { };
		};

	public:

		/**
		 * Constructor
		 */
		Sysctrl_general_core()
		:
			Mmio(Board::SYSCTRL_GENERAL_CORE_MMIO_BASE)
		{ }
};

class Omap4430::Sysctrl_general_core : public Omap::Sysctrl_general_core
{
	public:

		/**
		 * Return maximum frequency of the MPU clock for OMAP version 4430
		 */
		unsigned max_mpu_clk()
		{
			switch (read<Std_fuse_prod_id_1::Silicon_type>()) {
			case 0: return  800000000;
			case 1: return 1000000000;
			case 2: return 1200000000;
			default:
				PWRN("unknown silicon type");
				return 0;
			}
		}
};

class Omap4460::Sysctrl_general_core : public Omap::Sysctrl_general_core
{
	public:

		/**
		 * Return maximum frequency of the MPU clock for OMAP version 4460
		 */
		unsigned max_mpu_clk()
		{
			switch (read<Std_fuse_prod_id_1::Silicon_type>()) {
			case 1: return 1200000000;
			case 2: return 1500000000;
			default:
				PWRN("unknown silicon type");
				return 0;
			}
		}
};


/**
 * Return singleton of Cm1_region_a
 */
static Cm1_region_a * cm1_region_a()
{
	return unmanaged_singleton<Cm1_region_a>();
}


/**
 * Return singleton of Prm
 */
static Prm * prm()
{
	return unmanaged_singleton<Prm>();
}


/***********
 ** Board **
 ***********/

unsigned Board::max_mpu_clk()
{
	switch (omap_version()) {
	case omap4430():
		return Omap4430::sysctrl_general_core()->max_mpu_clk();
	case omap4460():
		return Omap4460::sysctrl_general_core()->max_mpu_clk();
	default:
		PWRN("unknown OMAP version");
		return 0;
	}
}

void Board::setup_max_mpu_clk()
{
	unsigned mult, div, clkout_div;
	unsigned const sys_clk = prm()->sys_clk();
	switch (max_mpu_clk()) {
	case 1200000000:
		switch (sys_clk) {
		case 38400000:
			mult       = 125;
			div        = 3;
			clkout_div = 1;
			break;
		default:
			PERR("unknown system clock");
			while (1) ;
		}
		break;
	case 1000000000:
		switch (sys_clk) {
		case 38400000:
			mult       = 625;
			div        = 23;
			clkout_div = 1;
			break;
		default:
			PERR("unknown system clock");
			while (1) ;
		}
		break;
	default:
		PERR("unknown MPU clock");
		while (1) ;
	}
	cm1_region_a()->setup_dpll_mpu(mult, div, clkout_div);
}


unsigned Board::omap_version()
{
	switch (Processor_driver::cortex_a9_version()) {
	case Processor_driver::cortex_a9_r0p1():
	case Processor_driver::cortex_a9_r1p2():
	case Processor_driver::cortex_a9_r1p3():  return omap4430();
	case Processor_driver::cortex_a9_r2p10(): return omap4460();
	default:
		PERR("unknown Cortex-A9 version");
		while (1) ;
	}
}


/**************
 ** Platform **
 **************/

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
		/* controls of cores timer and interrupt controller */
		{ Board::CORTEX_A9_PRIVATE_MEM_BASE,
		  Board::CORTEX_A9_PRIVATE_MEM_SIZE },

		/* controls of cores serial output */
		{ Board::TL16C750_3_MMIO_BASE, Board::TL16C750_MMIO_SIZE },

		/* l2 cache controller */
		{ Board::PL310_MMIO_BASE, Board::PL310_MMIO_SIZE },

		/* clock controls of the CPU and cores timer */
		{ Board::CM1_REGION_A_MMIO_BASE, Board::CM1_REGION_A_MMIO_SIZE },

		/* power and reset manager */
		{ Board::PRM_MMIO_BASE, Board::PRM_MMIO_SIZE },

		/* general system controls */
		{ Board::SYSCTRL_GENERAL_CORE_MMIO_BASE,
		  Board::SYSCTRL_GENERAL_CORE_MMIO_SIZE }
	};
	return i < sizeof(_regions)/sizeof(_regions[0]) ? &_regions[i] : 0;
}


Processor_driver::User_context::User_context() { cpsr = Psr::init_user(); }


static Board::Pl310 * l2_cache() {
	return unmanaged_singleton<Board::Pl310>(Board::PL310_MMIO_BASE); }


void Board::outer_cache_invalidate() { l2_cache()->invalidate(); }
void Board::outer_cache_flush()      { l2_cache()->flush();      }
void Board::prepare_kernel()         { l2_cache()->invalidate(); }
