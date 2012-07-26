/*
 * \brief  Declare interface of the expected singleton wishbone-slave
 * \author Martin Stein
 * \date   2012-07-15
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <stdint.h>

namespace Wishbone_slave
{
	                            /* used bit range */
	extern uint8_t  & rst_i();  /* [0:0]          */
	extern uint8_t  & clk_i();  /* [0:0]          */
	extern uint8_t  & stb_i();  /* [0:0]          */
	extern uint8_t  & cyc_i();  /* [0:0]          */
	extern uint32_t & adr_i();  /* [31:0]         */
	extern uint32_t & dat_i();  /* [31:0]         */
	extern uint8_t  & sel_i();  /* [3:0]          */
	extern uint8_t  & we_i();   /* [0:0]          */
	extern uint8_t  & ack_o();  /* [0:0]          */
	extern uint8_t  & err_o();  /* [0:0]          */
	extern uint8_t  & rty_o();  /* [0:0]          */
	extern uint32_t & dat_o();  /* [31:0]         */

	extern void evaluate_design();
}

