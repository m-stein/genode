/*
 * \brief  Virtual-machine session interface
 * \author Stefan Kalkowski
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__INCLUDE__VM_SESSION__VM_SESSION_H_
#define _BASE_HW__INCLUDE__VM_SESSION__VM_SESSION_H_

/* Genode includes */
#include <base/capability.h>
#include <base/exception.h>
#include <session/session.h>
#include <dataspace/capability.h>

namespace Genode
{

	/**
	 * Vm session interface
	 */
	struct Vm_session : Session
	{
		/**
		 * Returns the string that can be used to refer to this service
		 */
		static const char * service_name() { return "VM"; }


		class Region_conflict : public Exception { };


		/**
		 * Destructor
		 */
		virtual ~Vm_session() { }

		virtual Genode::Dataspace_capability dataspace() = 0;

		virtual void start() = 0;

		virtual void add_region(addr_t addr, size_t sz) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_dataspace, Genode::Dataspace_capability, dataspace);
		GENODE_RPC(Rpc_start, void, start);
		GENODE_RPC_THROW(Rpc_add_region, void, add_region,
		                 GENODE_TYPE_LIST(Region_conflict), addr_t, size_t);
		GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_start, Rpc_add_region);
	};
}

#endif /* _BASE_HW__INCLUDE__VM_SESSION__VM_SESSION_H_ */

