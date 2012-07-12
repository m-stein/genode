/*
 * \brief  Client-side vm session interface
 * \author Stefan Kalkowski
 * \date   2012-06-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__INCLUDE__VM_SESSION__CLIENT_H_
#define _BASE_HW__INCLUDE__VM_SESSION__CLIENT_H_

/* Genode includes */
#include <vm_session/capability.h>
#include <vm_session/vm_session.h>
#include <base/rpc_client.h>

namespace Genode
{
	/**
	 * Client-side signal session interface
	 */
	struct Vm_session_client : Rpc_client<Vm_session>
	{
		/**
		 * Constructor
		 */
		explicit Vm_session_client(Vm_session_capability const c)
		: Rpc_client<Vm_session>(c) { }


		/****************
		 ** Vm_session **
		 ****************/

		Genode::Dataspace_capability dataspace() {
			return call<Rpc_dataspace>(); }

		void start() {
			call<Rpc_start>(); }

		void add_region(addr_t addr, size_t sz) {
			call<Rpc_add_region>(addr, sz); }
	};
}

#endif /* _BASE_HW__INCLUDE__VM_SESSION__CLIENT_H_ */
