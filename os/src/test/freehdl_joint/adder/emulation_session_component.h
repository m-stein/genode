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

namespace Emulation
{
	/**
	 * Session component of the emulation service
	 */
	class Session_component : public Rpc_object<Session>
	{
		public:

			/**
			 * Construct a valid session component
			 */
			Session_component() { }


			/***********************
			 ** Session interface **
			 ***********************/

			void write_mmio(addr_t const off, Access const a,
			                umword_t const value)
			{
				printf("Write MMIO %lx, %u, %u\n", off, a, value);
			}

			umword_t read_mmio(addr_t const off, Access const a)
			{
				printf("Read MMIO %lx, %u\n", off, a);
				return 0;
			}

			bool irq_handler(unsigned const irq,
			                 Signal_context_capability irq_edge)
			{
				PERR("%s:%d: Invalid IRQ", __FILE__, __LINE__);
				return 0;
			}
	};
}

#endif /* _ADDER__EMULATION_SESSION_COMPONENT_H_ */

