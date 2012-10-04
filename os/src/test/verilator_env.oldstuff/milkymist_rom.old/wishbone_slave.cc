/*
 * \brief   Implementation of the wishbone-slave interface
 * \author  Martin Stein
 * \date    2012-07-25
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include "Vtop.h"

/* Wishbone-lib includes */
#include <wishbone_slave.h>

static Vtop * design()
{
	static Vtop _design;
	return &_design;
}

namespace Wishbone_slave
{
	uint8_t  & rst_i() { return design()->rst_i; };
	uint8_t  & clk_i() { return design()->clk_i; };
	uint8_t  & stb_i() { return design()->stb_i; };
	uint8_t  & cyc_i() { return design()->cyc_i; };
	uint32_t & adr_i() { return design()->adr_i; };
	uint32_t & dat_i() { return design()->dat_i; };
	uint8_t  & sel_i() { return design()->sel_i; };
	uint8_t  & we_i()  { return design()->we_i; };
	uint8_t  & ack_o() { return design()->ack_o; };
	uint8_t  & err_o() { return design()->err_o; };
	uint8_t  & rty_o() { return design()->rty_o; };
	uint32_t & dat_o() { return design()->dat_o; };

	void evaluate_design() { design()->eval(); };
}

