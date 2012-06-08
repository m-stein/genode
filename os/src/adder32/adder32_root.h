/*
 * \brief   Root component of the 32-bit-adder-emulator service
 * \author  Martin Stein
 * \date    2012-05-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__ADDER32__ADDER32_ROOT_H_
#define _SRC__ADDER32__ADDER32_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* Local includes */
#include "adder32_session_component.h"

namespace Genode
{
	/**
	 * Root component of a 32-bit-adder-emulation service
	 */
	class Adder32_root : public Root_component<Adder32_session_component>
	{
		bool _session_exists; /* Allow only one session per adder */

		/**
		 * Create a new session for the requested file
		 *
		 * \param   args  Session arguments
		 * \throws        Multiple_sessions_not_allowed
		 */
		Adder32_session_component * _create_session(const char *args)
		{
			if(_session_exists) throw Multiple_sessions_not_allowed();
			_session_exists = true;
			return new (md_alloc()) Adder32_session_component();
		}

		public:

			/* Exceptions */
			class Multiple_sessions_not_allowed : public Exception { };

			/**
			 * Constructor
			 *
			 * \param  ep  entrypoint for root component
			 * \param  md  meta-data allocator for root component
			 */
			Adder32_root(Rpc_entrypoint * const ep,
			             Allocator * const md)
			:
				Root_component<Adder32_session_component>(ep, md),
				_session_exists(false)
			{ }
	};
}

#endif /* _SRC__ADDER32__ADDER32_ROOT_H_ */

