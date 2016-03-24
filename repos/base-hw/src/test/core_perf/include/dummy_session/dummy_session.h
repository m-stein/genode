/*
 * \brief  Dummy session interface
 * \author Martin Stein
 * \date   2016-03-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DUMMY_SESSION__DUMMY_SESSION_H_
#define _DUMMY_SESSION__DUMMY_SESSION_H_

/* Genode includes */
#include <session/session.h>
#include <base/signal.h>

namespace Dummy
{
	using Genode::Signal_context_capability;

	struct Session;
}


struct Dummy::Session : Genode::Session
{
	static const char *service_name() { return "Dummy"; }

	virtual ~Session() { }

	virtual int dummy_rpc(int x) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dummy_rpc, int, dummy_rpc, int);

	GENODE_RPC_INTERFACE(Rpc_dummy_rpc);
};

#endif /* _DUMMY_SESSION__DUMMY_SESSION_H_ */
