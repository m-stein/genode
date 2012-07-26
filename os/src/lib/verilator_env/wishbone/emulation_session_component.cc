/*
 * \brief   Bridge emulation-session to a verilated wishbone slave
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
#include <emulation_session_component.h>
#include <rm_session/rm_session.h>

/* local includes */
#include "wishbone_slave.h"

/* Verilator includes */
#include <verilated.h>

using namespace Wishbone_slave;
using namespace Genode;

enum { MAX_SLAVE_CYCLES = 1000 };


/***********************
 ** Utility functions **
 ***********************/

/**
 * Evaluate a verilated design once
 */
static void evaluate()
{
	assert(!Verilated::gotFinish());
	evaluate_design();
}


/**
 * Do a clock cycle at a wishbone slave
 */
static void cycle()
{
	/* apply low clock state (may imply a falling edge) */
	clk_i() = 0;
	evaluate();

	/* do a rising edge at the clock */
	clk_i() = 1;
	evaluate();
}

/**
 * Do a reset cycle at a wishbone slave
 */
static void reset()
{
	/* apply low reset signal */
	rst_i() = 0;
	evaluate();

	/* raise reset and simulate reaction of the master */
	rst_i() = 1;
	cyc_i() = 0;
	stb_i() = 0;
	evaluate();

	/* end reset cycle */
	rst_i() = 0;
}


/**********************************
 ** Emulation::Session_component **
 **********************************/

Emulation::Session_component::Session_component() { reset(); }


umword_t Emulation::Session_component::read_mmio(addr_t const addr, Access const a)
{
	/* apply transfer parameters to the bus */
	adr_i() = (uint32_t)addr;
	we_i() = 0;
	switch(a) {
	case Rm_session::LSB8:  sel_i() = 0b0001; break;
	case Rm_session::LSB16: sel_i() = 0b0011; break;
	case Rm_session::LSB32: sel_i() = 0b1111; break;
	default: assert(0);
	}
	/* do a transfer-start cycle at the bus */
	stb_i() = 1;
	cyc_i() = 1;
	cycle();

	/* wait for feedback from the slave  */
	unsigned slave_cycles = 0;
	while (1) {
		if (ack_o())
		{
			/* buffer read value */
			umword_t const result = dat_o();
			Genode::printf("READ %lx %lx\n", addr, result);

			/* do a transfer-end cycle */
			/* FIXME might be unnecessary */
			cyc_i() = 0;
			stb_i() = 0;
			cycle();
			return result;
		}
		/* check conditions to continue waiting for feedback */
		assert(!err_o() && !rty_o());
		if (MAX_SLAVE_CYCLES) {
			assert(slave_cycles < MAX_SLAVE_CYCLES);
			slave_cycles++;
		}
		cycle();
	}
}


void
Emulation::Session_component::write_mmio(addr_t const addr, Access const a,
                                         umword_t const value)
{
	/* apply transfer parameters to the bus */
	adr_i() = (uint32_t)addr;
	dat_i() = (uint32_t)value;
	we_i() = 1;
	switch(a) {
	case Rm_session::LSB8:  sel_i() = 0b0001; break;
	case Rm_session::LSB16: sel_i() = 0b0011; break;
	case Rm_session::LSB32: sel_i() = 0b1111; break;
	default: assert(0);
	}
	/* do a transfer-start cycle at the bus */
	stb_i() = 1;
	cyc_i() = 1;
	cycle();

	/* wait for feedback from the slave  */
	unsigned slave_cycles = 0;
	while (1) {
		if (ack_o())
		{
			Genode::printf("WRITE %lx %x\n", addr, dat_o());

			/* do a transfer-end cycle */
			/* FIXME might be unnecessary */
			cyc_i() = 0;
			stb_i() = 0;
			cycle();
			return;
		}
		/* check conditions to continue waiting for feedback */
		assert(!err_o() && !rty_o());
		if (MAX_SLAVE_CYCLES) {
			assert(slave_cycles < MAX_SLAVE_CYCLES);
			slave_cycles++;
		}
		cycle();
	}
}

bool Emulation::Session_component::irq_handler(unsigned const,
                                               Signal_context_capability)
{
	PWRN("IRQ handling not supported");
	return 0;
};

