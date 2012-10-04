/*
 * \brief   Session component of the emulation service
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ADDER__EMULATION_SESSION_COMPONENT_H_
#define _ADDER__EMULATION_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/printf.h>
#include <base/sleep.h>

/* local includes */
#include <emulation_session/emulation_session.h>

/* verilator includes */
#include "Vtop.h"
#include "verilated.h"

namespace Emulation
{
	/**
	 * Session component of the emulation service
	 *
	 * Assumes that 'Vtop' is a the top-component class of a design that has
	 * gone through 'verilator' to create C++ code from verilog. It is assumed
	 * furthermore that it provides a wishbone-slave interface with, at least,
	 * the following signals: 'clk_i', 'rst_i', 'cyc_i', 'stb_i',
	 * 'adr_i[31:0]', 'dat_i[31:0]', 'sel_i[3:0]', 'dat_o[31:0]', 'ack_o',
	 * 'err_o' and 'rty_o'. This component then communicates with 'Vtop' as
	 * slave by following the wishbone protocol. It thereby supports only
	 * single read/write cycles without the use of 'err_o', 'ack_o',
	 * 'tgd_i', 'tga_i' and 'tgc_i'.
	 * However, even though the use of some of them is not permitted by this
	 * component, 'Vtop' has to provide all stated signals.
	 */
	class Session_component : public Rpc_object<Session>
	{
		Vtop * top;

		/**
		 * Do a bus cycle
		 *
		 * Propagates a low state (may imply a down edge) and subsequently
		 * an up edge at the slaves 'clk_i' input.
		 */
		_bus_cycle()
		{
			top->clk_i = 0;
			top->eval();
			top->clk_i = 1;
			top->eval();
		}

		public:

			/**
			 * Construct a valid session component
			 */
			Session_component() : top(new Vtop)
			{
				/* do reset operation at bus interface */
				top->rst_i = 1;
				top->stb_i = 0;
				top->cyc_i = 0;
				_bus_cycle();
				top->rst_i = 0;
			}


			/***********************
			 ** Session interface **
			 ***********************/

			void write_mmio(addr_t const off, Access const a,
			                umword_t const value)
			{
				Genode::printf("write MMIO %lx, %u, %lu\n", off, a, value);
				enum { MAX_SLAVE_CYCLES = 1000 };
				unsigned slave_cycles = 0;
				assert(!Verilated::gotFinish());

				/* apply transfer parameters to the bus */
				top->dat_i = (uint32_t)value;
				top->adr_i = (uint32_t)off;
				top->we_i = 1;
				switch(a) {
				case Rm_session::LSB8:  top->sel_i = 0b0001; break;
				case Rm_session::LSB16: top->sel_i = 0b0011; break;
				case Rm_session::LSB32: top->sel_i = 0b1111; break;
				default: assert(0);
				}
				/* do a transfer-start cycle at the bus */
				top->stb_i = 1;
				top->cyc_i = 1;
				_bus_cycle();

				/* wait for feedback from the slave  */
				while (1) {
					_bus_cycle();
					if (top->ack_o)
					{
						/* do a transfer-end cycle */
						top->cyc_i = 0
						top->stb_i = 0
						_bus_cycle();
						break;
					}
					/* check conditions to continue waiting for feedback */
					assert(!top->err_o && !top->rty_o);
					assert(slave_cycles < MAX_SLAVE_CYCLES);
					slave_cycles++;
				}
			}

			umword_t read_mmio(addr_t const off, Access const a)
			{
				PERR("Not supported");
				return 0;
			}

			bool irq_handler(unsigned const irq,
			                 Signal_context_capability irq_edge)
			{
				Genode::printf("Handle IRQ %u\n", irq);
				return 0;
			}
	};
}

#endif /* _ADDER__EMULATION_SESSION_COMPONENT_H_ */

