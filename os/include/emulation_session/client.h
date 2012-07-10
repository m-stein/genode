/*
 * \brief   Client of an emulation session
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EMULATION_SESSION__CLIENT_H_
#define _INCLUDE__EMULATION_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>

/* local includes */
#include "capability.h"
#include "emulation_session.h"

namespace Emulation
{
	using namespace Genode;

	/**
	 * Client of an emulation session
	 */
	struct Session_client : Rpc_client<Session>
	{
		/**
		 * Constructor
		 */
		explicit Session_client(Session_capability s) : Rpc_client<Session>(s)
		{ }

		/***********************
		 ** Emulation::Session **
		 ***********************/

		void write_mmio(addr_t const o, Access const a, umword_t const v)
		{ call<Rpc_write_mmio>(o, a, v); }

		umword_t read_mmio(addr_t const o, Access const a)
		{ return call<Rpc_read_mmio>(o, a); }

		bool irq_handler(unsigned const irq,
		                 Signal_context_capability irq_edge)
		{ return call<Rpc_irq_handler>(irq, irq_edge); }
	};
}

#endif /* _INCLUDE__EMULATION_SESSION__CLIENT_H_ */

