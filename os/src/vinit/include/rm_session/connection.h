/*
 * \brief  Connection to a local RM service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__CONNECTION_H_
#define _INCLUDE__RM_SESSION__CONNECTION_H_

/* local includes */
#include <local_connection.h>
#include <rm_session/root.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Connection to a local RM service
	 */
	class Rm_connection : public Local_connection<Rm_session_component>,
	                      public Rm_session_client
	{
		public:

			enum { RAM_QUOTA = 64 * 1024 };

			/**
			 * Constructor
			 *
			 * \param  root   local root wich serves RM sessions
			 * \param  start  start of the managed virtual memory-region
			 * \param  size   size of the virtual memory-region to manage
			 */
			Rm_connection(Rm_root * root, addr_t start = ~0UL, size_t size = 0) :
				Local_connection(root, session(root, "ram_quota=%u,"
				                               " start=0x%p, size=0x%x",
				                               RAM_QUOTA, start, size)),
				Rm_session_client(cap())
			{ }
	};
}

#endif /* _INCLUDE__RM_SESSION__CONNECTION_H_ */

