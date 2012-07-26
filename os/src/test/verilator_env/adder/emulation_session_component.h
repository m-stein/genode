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

/* local includes */
#include <emulation_session/emulation_session.h>

/* verilator includes */
#include "Vtop.h"
#include "verilated.h"

namespace Emulation
{
	/**
	 * Session component of the emulation service
	 */
	class Session_component : public Rpc_object<Session>
	{
		Vtop * top;

		public:

			/**
			 * Construct a valid session component
			 */
			Session_component() : top(new Vtop)
			{
				top->addend_1 = 0;
				top->addend_2 = 0;
				top->carry_in = 0;
			}


			/***********************
			 ** Session interface **
			 ***********************/

			void write_mmio(addr_t const off, Access const a,
			                umword_t const value)
			{
				Genode::printf("write MMIO %lx, %u, %lu\n", off, a, value);
				assert(!Verilated::gotFinish());
				switch (off) {
				case 0x0: {
					top->addend_1 = (bool)(value & 1);
					break; }
				case 0x4: {
					top->addend_2 = (bool)(value & 1);
					break; }
				case 0x8: {
					top->carry_in = (bool)(value & 1);
					break; }
				default: {
					PWRN("invalid write offset");
					break; }
				}
				top->eval();
			}

			umword_t read_mmio(addr_t const off, Access const a)
			{
				Genode::printf("Read MMIO %lx, %u\n", off, a);
				switch (off) {
				case 0xc: return top->sum;
				case 0x10: return top->carry_out;
				default: {
					PWRN("invalid read offset");
					return 0; }
				}
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

