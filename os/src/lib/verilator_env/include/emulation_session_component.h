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

#ifndef _VERILATOR_ENV__INCLUDE__EMULATION_SESSION_COMPONENT_H_
#define _VERILATOR_ENV__INCLUDE__EMULATION_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/printf.h>
#include <base/sleep.h>
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
			Session_component();

			/***********************
			 ** Session interface **
			 ***********************/

			void write_mmio(addr_t const, Access const, umword_t const);

			umword_t read_mmio(addr_t const, Access const);

			bool irq_handler(unsigned const, Signal_context_capability);
	};
}

#endif /* _VERILATOR_ENV__INCLUDE__EMULATION_SESSION_COMPONENT_H_ */

