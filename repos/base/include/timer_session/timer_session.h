/*
 * \brief  Timer session interface
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TIMER_SESSION__TIMER_SESSION_H_
#define _INCLUDE__TIMER_SESSION__TIMER_SESSION_H_

#include <base/signal.h>
#include <session/session.h>

namespace Timer { struct Session; }


struct Timer::Session : Genode::Session
{
	typedef Genode::Signal_context_capability Signal_context_capability;

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Timer"; }

	enum { CAP_QUOTA = 2 };

	virtual ~Session() { }

	/**
	 * Program single timeout (relative from now in microseconds)
	 */
	virtual void xrigger_once(Genode::uint64_t us) = 0;

	/**
	 * Program periodic timeout (in microseconds)
	 *
	 * The first period will be triggered after 'us' at the latest,
	 * but it might be triggered earlier as well.
	 */
	virtual void xrigger_periodic(Genode::uint64_t us) = 0;

	/**
	 * Register timeout signal handler
	 */
	virtual void sigh(Genode::Signal_context_capability sigh) = 0;

	/**
	 * Return number of elapsed milliseconds since session creation
	 */
	virtual Genode::uint64_t xlapsed_ms() const = 0;

	virtual Genode::uint64_t xlapsed_us() const = 0;

	/**
	 * Client-side convenience method for sleeping the specified number
	 * of milliseconds
	 */
	virtual void mxleep(Genode::uint64_t ms) = 0;

	/**
	 * Client-side convenience method for sleeping the specified number
	 * of microseconds
	 */
	virtual void uxleep(Genode::uint64_t us) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_xrigger_once, void, xrigger_once, Genode::uint64_t);
	GENODE_RPC(Rpc_xrigger_periodic, void, xrigger_periodic, Genode::uint64_t);
	GENODE_RPC(Rpc_sigh, void, sigh, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_xlapsed_ms, Genode::uint64_t, xlapsed_ms);
	GENODE_RPC(Rpc_xlapsed_us, Genode::uint64_t, xlapsed_us);

	GENODE_RPC_INTERFACE(Rpc_xrigger_once, Rpc_xrigger_periodic,
	                     Rpc_sigh, Rpc_xlapsed_ms, Rpc_xlapsed_us);
};

#endif /* _INCLUDE__TIMER_SESSION__TIMER_SESSION_H_ */
