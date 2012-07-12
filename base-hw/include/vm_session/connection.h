/*
 * \brief  Connection to VM service
 * \author Stefan Kalkowski
 * \date   2012-06-22
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__INCLUDE__VM_SESSION__CONNECTION_H_
#define _BASE_HW__INCLUDE__VM_SESSION__CONNECTION_H_

#include <vm_session/client.h>
#include <base/connection.h>

namespace Genode {

	struct Vm_connection : Connection<Vm_session>, Vm_session_client
	{
			Vm_connection()
			: Connection<Vm_session>(session("ram_quota=12K")),
			  Vm_session_client(cap()) { }
	};

}

#endif /* _INCLUDE__VM_SESSION__CONNECTION_H_ */
