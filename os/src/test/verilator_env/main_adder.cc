/*
 * \brief   Test device that is in fact an emulated verilog design
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
#include <base/printf.h>
#include <io_mem_session/connection.h>

using namespace Genode;

/**
 * Adder driver
 */
class Adder : public Mmio
{
	/**
	 * Addition argument 1
	 */
	struct Addend_1 : Register<0x0, 1> { };

	/**
	 * Addition argument 2
	 */
	struct Addend_2 : Register<0x4, 1> { };

	/**
	 * Incoming carry bit
	 */
	struct Carry_in : Register<0x8, 1> { };

	/**
	 * Addition result
	 */
	struct Sum : Register<0xc, 1> { };

	/**
	 * Resulting carry bit
	 */
	struct Carry_out : Register<0x10, 1> { };

	public:

		enum { MMIO_SIZE = 0x1000 };

		/**
		 * Construct a new driver
		 */
		Adder(addr_t const mmio_base) : Mmio(mmio_base) { }

		/**
		 * Add two unsigned 8-bit integers
		 *
		 * \param a, b  terms of the sum
		 */
		char sum(char const a, char const b)
		{
			bool carry = 0;
			char sum = 0;
			for(int i = 0; i < 8; i++)
			{
				write<Addend_1>((bool)((a >> i) & 1));
				write<Addend_2>((bool)((b >> i) & 1));
				write<Carry_in>(carry);
				sum = sum + (read<Sum>() << i);
				carry = read<Carry_out>();
			}
			return sum;
		}
};


/**
 * Main routine
 */
int main(int argc, char **argv)
{
	printf("--- test device that is in fact an emulated verilog design ---\n");

	/* initialize device driver */
	enum { ADDER_MMIO_BASE = 0x70000000 };
	static Rm_session * const rm = env()->rm_session();
	static Io_mem_connection adder_mmio(ADDER_MMIO_BASE, Adder::MMIO_SIZE);
	static Adder adder((addr_t)rm->attach(adder_mmio.dataspace()));

	/* do some calculations with the device */
	printf("%u + %u = %u\n", 14, 34, adder.sum(14, 34));
}

