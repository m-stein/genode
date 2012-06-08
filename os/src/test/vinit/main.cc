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

using namespace Genode;

/**
 * Driver for a 32-bit adder
 */
class Adder32
{
	/* MMIO registers */
	volatile uint32_t * const _addend1;
	volatile uint32_t * const _addend2;
	volatile uint32_t * const _sum;

	public:

		enum { MMIO_SIZE = 3*sizeof(uint32_t) };

		/**
		 * Constructor
		 */
		Adder32(addr_t const base) :
			_addend1((volatile uint32_t *)base + 0),
			_addend2((volatile uint32_t *)base + 1),
			_sum((volatile uint32_t *)base + 2)
		{ }

		/**
		 * Add two unsigned integers
		 *
		 * \param  a,b  Terms of the sum
		 */
		uint32_t sum(uint32_t const a, uint32_t const b)
		{
			*_addend1 = a;
			*_addend2 = b;
			return *_sum;
		}
};


/**
 * Main routine
 */
int main(int argc, char **argv)
{
	/* Initialize adder driver */
	printf("--- test driver for the 32-bit adder ---\n");
	static Io_mem_connection adder_mmio(0x71000000, Adder32::MMIO_SIZE);
	static Rm_session * const rm = env()->rm_session();
	static Adder32 adder((addr_t)rm->attach(adder_mmio.dataspace()));

	/* Do some additions on the adder */
	static unsigned i=2;
	static unsigned j=1;
	static unsigned k=0;
	for(; j<=10;) {
		k = adder.sum(i,j);
		printf("%u + %u = %u\n", i, j, k);
		j = k;
	}
	sleep_forever();
}

