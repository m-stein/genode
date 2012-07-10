/*
 * \brief   Test user for the 32-bit-adder-emulator service
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <io_mem_session/connection.h>
#include <irq_session/connection.h>

using namespace Genode;

/**
 * FPU driver
 */
class Fpu
{
	/**
	 * Structure of first emulated MMIO region
	 */
	class Mmio_1 : public Mmio
	{
		public:

			/**
			 * Control register
			 */
			struct Ctrl : Register<0x0, 32>
			{
				struct Mode : Bitfield<0, 1> /* calulation mode */
				{
					enum { ADD = 0, SUBTRACT = 1 };
				};
				struct Calc : Bitfield<1, 1> { }; /* calculation active bit */
				struct Clr_irq : Bitfield<2, 1> { }; /* clear interrupt */

				/**
				 * Opcode that starts an addition
				 */
				static access_t do_add()
				{ return Mode::bits(Mode::ADD) | Calc::bits(1); }

				/**
				 * Opcode that starts a subtraction
				 */
				static access_t do_subtract()
				{ return Mode::bits(Mode::SUBTRACT) | Calc::bits(1); }
			};

			/**
			 * Constructor
			 */
			Mmio_1(addr_t const base) : Mmio(base) { }
	};

	/**
	 * Structure of second emulated MMIO region
	 */
	class Mmio_2 : public Mmio
	{
		public:

			/**
			 * Calculation argument 1
			 */
			struct Arg_1 : Register<0x0, 32> { };

			/**
			 * Calculation argument 2
			 */
			struct Arg_2 : Register<0x4, 32> { };

			/**
			 * Calculation result
			 */
			struct Result : Register<0x8, 32> { };

			/**
			 * Constructor
			 */
			Mmio_2(addr_t const base) : Mmio(base) { }
	};

	Mmio_1 _mmio_1; /* first device MMIO region */
	Mmio_2 _mmio_2; /* second device MMIO region */
	Irq_session * const _irq; /* single IRQ of the device */

	public:

		enum {
			MMIO_1_SIZE = 0x1000,
			MMIO_2_SIZE = 0x1000,
		};

		/**
		 * Construct a new driver
		 */
		Fpu(addr_t const mmio_1_base, addr_t const mmio_2_base,
		    Irq_session * const irq)
		: _mmio_1(mmio_1_base), _mmio_2(mmio_2_base), _irq(irq) { }

		/**
		 * Add two unsigned integers
		 *
		 * \param a,b  terms of the sum
		 */
		unsigned sum(unsigned const a, unsigned const b)
		{
			_mmio_2.write<Mmio_2::Arg_1>(a);
			_mmio_2.write<Mmio_2::Arg_2>(b);
			_mmio_1.write<Mmio_1::Ctrl>(Mmio_1::Ctrl::do_add());
			_irq->wait_for_irq();
			_mmio_1.write<Mmio_1::Ctrl::Clr_irq>(1);
			return _mmio_2.read<Mmio_2::Result>();
		}

		/**
		 * Add two unsigned integers
		 *
		 * \param a,b  terms of the sum
		 */
		unsigned difference(unsigned const a, unsigned const b)
		{
			_mmio_2.write<Mmio_2::Arg_1>(a);
			_mmio_2.write<Mmio_2::Arg_2>(b);
			_mmio_1.write<Mmio_1::Ctrl>(Mmio_1::Ctrl::do_subtract());
			_irq->wait_for_irq();
			_mmio_1.write<Mmio_1::Ctrl::Clr_irq>(1);
			return _mmio_2.read<Mmio_2::Result>();
		}
};


/**
 * Main routine
 */
int main(int argc, char **argv)
{
	printf("--- test device emulation through vinit ---\n");

	/* initialize device drivers */
	enum {
		FPU_MMIO_1_BASE = 0x71000000,
		FPU_MMIO_2_BASE = 0x71001000,
		FPU_IRQ = 2000,
	};
	static Rm_session * const rm = env()->rm_session();
	static Io_mem_connection fpu_mmio_1(FPU_MMIO_1_BASE, Fpu::MMIO_1_SIZE);
	static Io_mem_connection fpu_mmio_2(FPU_MMIO_2_BASE, Fpu::MMIO_2_SIZE);
	static Irq_connection fpu_irq(FPU_IRQ);
	static Fpu fpu((addr_t)rm->attach(fpu_mmio_1.dataspace()),
	               (addr_t)rm->attach(fpu_mmio_2.dataspace()), &fpu_irq);

	/* do some calculations with the devices */
	static unsigned a = 3;
	static unsigned b = 2;
	static unsigned sum = 0;
	static unsigned diff = 0;
	for(int i = 0; i < 5; i++) {
		sum = fpu.sum(a,b);
		diff = fpu.difference(a,b);

		/* validate calculation result */
		if (sum != a + b || diff != a - b) {
			PERR("%s:%d: Calculation failure", __FILE__, __LINE__);
			sleep_forever();
		}
		printf("%u + %u = %u\n", a, b, sum);
		printf("%u - %u = %u\n", a, b, diff);

		/* prepare next calculation */
		a = sum;
		b = diff;
	}
	sleep_forever();
}

