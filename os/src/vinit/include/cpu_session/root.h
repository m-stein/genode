/*
 * \brief  Root component of an eavesdropping CPU service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__ROOT_H_
#define _INCLUDE__CPU_SESSION__ROOT_H_

/* local includes */
#include <spy_root_component.h>
#include <cpu_session/component.h>

namespace Init
{
	/**
	 * Root component of an eavesdropping CPU service
	 */
	class Cpu_root : public Spy_root_component<Cpu_session_component>
	{
		public:

			/**
			 * Constructor
			 *
			 * \param  ep  entrypoint for 'Root'
			 * \param  md  metadata allocator for 'Root'
			 */
			Cpu_root(Rpc_entrypoint * const ep, Allocator * const md)
			: Spy_root_component<Cpu_session_component>(ep, md) { }
	};
}

#endif /* _INCLUDE__CPU_SESSION__ROOT_H_ */

