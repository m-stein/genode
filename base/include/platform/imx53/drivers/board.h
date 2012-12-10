/*
 * \brief  Board definitions for the i.MX53
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__BOARD_H_
#define _INCLUDE__PLATFORM__BOARD_H_

/**
 * Genode includes
 */
#include <util/mmio.h>

namespace Genode
{
	class Aipstz : public Mmio
	{
		/**
		 * Configuration of the masters
		 */
		struct Mpr { enum { ALL_UNBUFFERED_AND_FULLY_TRUSTED = 0x77777777 }; };
		struct Mpr1 : Register<0x0, 32>, Mpr { };
		struct Mpr2 : Register<0x4, 32>, Mpr { };

		/**
		 * Configuration of the platform peripherals
		 */
		struct Pacr { enum { ALL_UNBUFFERED_AND_FULLY_UNPROTECTED = 0 }; };
		struct Pacr1 : Register<0x20, 32>, Pacr { };
		struct Pacr2 : Register<0x24, 32>, Pacr { };
		struct Pacr3 : Register<0x28, 32>, Pacr { };
		struct Pacr4 : Register<0x2c, 32>, Pacr { };

		/**
		 * Configuration of the off-platform peripherals
		 */
		struct Opacr1 : Register<0x40, 32>, Pacr { };
		struct Opacr2 : Register<0x44, 32>, Pacr { };
		struct Opacr3 : Register<0x48, 32>, Pacr { };
		struct Opacr4 : Register<0x4c, 32>, Pacr { };
		struct Opacr5 : Register<0x50, 32>, Pacr { };

		public:

			/**
			 * Constructor
			 */
			Aipstz(addr_t const base) : Mmio(base) { }

			/**
			 * Configure this module appropriately for the first kernel run
			 */
			void prepare_kernel()
			{
				/* avoid AIPS intervention at any memory access */
				write<Mpr1>(Mpr::ALL_UNBUFFERED_AND_FULLY_TRUSTED);
				write<Mpr2>(Mpr::ALL_UNBUFFERED_AND_FULLY_TRUSTED);
				write<Pacr1>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Pacr2>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Pacr3>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Pacr4>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Opacr1>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Opacr2>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Opacr3>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Opacr4>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
				write<Opacr5>(Pacr::ALL_UNBUFFERED_AND_FULLY_UNPROTECTED);
			}
	};


	/**
	 * i.MX53 motherboard
	 */
	struct Board
	{
		enum {
			MMIO_BASE = 0x0,
			MMIO_SIZE = 0x70000000,

			CSD0_DDR_RAM_BASE  = 0x70000000,
			CSD0_DDR_RAM_SIZE  = 0x40000000,

			UART_1_IRQ         = 31,
			UART_1_MMIO_BASE   = 0x53fbc000,
			UART_1_MMIO_SIZE   = 0x00004000,

			EPIT_1_IRQ         = 40,
			EPIT_1_MMIO_BASE   = 0x53fac000,
			EPIT_1_MMIO_SIZE   = 0x00004000,

			EPIT_2_IRQ         = 41,
			EPIT_2_MMIO_BASE   = 0x53fb0000,
			EPIT_2_MMIO_SIZE   = 0x00004000,

			TZIC_MMIO_BASE     = 0x0fffc000,
			TZIC_MMIO_SIZE     = 0x00004000,

			AIPS_1_MMIO_BASE   = 0x53f00000,
			AIPS_2_MMIO_BASE   = 0x63f00000,

			SECURITY_EXTENSION = 1,
		};

		/**
		 * static AIPS 1 instance
		 */
		static Aipstz * aips_1() { static Aipstz a(AIPS_1_MMIO_BASE); return &a; }

		/**
		 * Static AIPS 2 instance
		 */
		static Aipstz * aips_2() { static Aipstz a(AIPS_2_MMIO_BASE); return &a; }

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void prepare_kernel()
		{
			aips_1()->prepare_kernel();
			aips_2()->prepare_kernel();
		}
	};
}

#endif /* _INCLUDE__PLATFORM__BOARD_H_ */

