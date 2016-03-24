/*
 * \brief  Client-side Dummy-session interface
 * \author Martin Stein
 * \date   2016-03-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DUMMY_SESSION__CLIENT_H_
#define _DUMMY_SESSION__CLIENT_H_

/* Genode includes */
#include <base/rpc_client.h>

/* local includes */
#include <dummy_session/capability.h>

namespace Dummy
{
	using Genode::Rpc_client;

	struct Session_client;
}


struct Dummy::Session_client : Rpc_client<Session>
{
	explicit Session_client(Session_capability session)
	: Rpc_client<Session>(session) { }

	int dummy_rpc(int x) override {
		return call<Rpc_dummy_rpc>(x); }
};

#endif /* _DUMMY_SESSION__CLIENT_H_ */
