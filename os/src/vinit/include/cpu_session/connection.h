/*
 * \brief  Connection to a local CPU service
 * \author Martin Stein
 * \date   2012-06-12
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__CONNECTION_H_
#define _INCLUDE__CPU_SESSION__CONNECTION_H_

/* Genode includes */
#include <cpu_session/connection.h>

/* local includes */
#include <cpu_session/root.h>
#include <local_connection.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Connection to a local CPU service
	 */
	class Cpu_connection : public Local_connection<Cpu_session_component>,
	                       public Cpu_session_client
	{
		public:

			enum { RAM_QUOTA = 256 * 1024 };

			/**
			 * Constructor
			 *
			 * \param  root      local root wich serves CPU sessions
			 * \param  label     initial session label
			 * \param  priority  designated priority of all threads created
			 *                   with this CPU session
			 */
			Cpu_connection(Cpu_root * const root, const char * label = "",
			               long priority = DEFAULT_PRIORITY)
			:
				Local_connection(root, session(root, "priority=0x%lx,"
				                               " ram_quota=%u, label=\"%s\"",
				                               priority, RAM_QUOTA, label)),
				Cpu_session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__CPU_SESSION__CONNECTION_H_ */

