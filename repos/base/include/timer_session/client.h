/*
 * \brief  Client-side timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-05-31
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TIMER_SESSION__CLIENT_H_
#define _INCLUDE__TIMER_SESSION__CLIENT_H_

#include <timer_session/capability.h>
#include <base/rpc_client.h>

namespace Timer { struct Session_client; }


struct Timer::Session_client : Genode::Rpc_client<Session>
{
	explicit Session_client(Session_capability session)
	: Genode::Rpc_client<Session>(session) { }

	void xrigger_once(Genode::uint64_t us) override { call<Rpc_xrigger_once>(us); }

	void xrigger_periodic(Genode::uint64_t us) override { call<Rpc_xrigger_periodic>(us); }

	void sigh(Signal_context_capability sigh) override { call<Rpc_sigh>(sigh); }

	Genode::uint64_t xlapsed_ms() const override { return call<Rpc_xlapsed_ms>(); }

	Genode::uint64_t xlapsed_us() const override { return call<Rpc_xlapsed_us>(); }
};

#endif /* _INCLUDE__TIMER_SESSION__CLIENT_H_ */
