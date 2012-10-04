/*
 * \brief   Root component of the emulation service
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__LIB__VERILATOR_ENV__EMULATION_ROOT_H_
#define _SRC__LIB__VERILATRO_ENV__EMULATION_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* local includes */
#include "emulation_session_component.h"

namespace Emulation
{
	/**
	 * Root component of the emulation service
	 */
	class Root : public Root_component<Session_component>
	{
		/**
		 * Create a new session
		 *
		 * \param  args  session arguments
		 * \throws       Multiple_sessions_not_allowed
		 */
		Session_component * _create_session(const char * args)
		{
			return new (md_alloc()) Session_component();
		}

		public:

			/**
			 * Construct a valid service root
			 *
			 * \param ep        entrypoint for the root component
			 * \param md_alloc  meta-data allocator for the root component
			 */
			Root(Rpc_entrypoint * const ep, Allocator * const md_alloc)
			: Root_component<Session_component>(ep, md_alloc)
			{ }
	};
}

#endif /* _SRC__LIB__VERILATRO_ENV__EMULATION_ROOT_H_ */

