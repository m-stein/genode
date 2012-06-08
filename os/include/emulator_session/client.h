/*
 * \brief   Client of an emulator session
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__EMULATOR_SESSION__CLIENT_H_
#define _INCLUDE__EMULATOR_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>

/* local includes */
#include "capability.h"
#include "emulator_session.h"

namespace Emulator
{
	/**
	 * Client of an emulator session
	 */
	struct Session_client : Genode::Rpc_client<Session>
	{
		/**
		 * Constructor
		 */
		explicit Session_client(Session_capability s) : Rpc_client<Session>(s)
		{ }

		/***********************
		 ** Emulator::Session **
		 ***********************/

		void write_mmio(addr_t const o, Access const a, umword_t const v)
		{ call<Rpc_write_mmio>(o, a, v); }

		umword_t read_mmio(addr_t const o, Access const a)
		{ return call<Rpc_read_mmio>(o, a); }
	};
}

#endif /* __INCLUDE__EMULATOR_SESSION__CLIENT_H_ */

