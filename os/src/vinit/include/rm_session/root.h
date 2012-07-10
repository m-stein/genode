/*
 * \brief  Root component of an eavesdropping RM service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__ROOT_H_
#define _INCLUDE__RM_SESSION__ROOT_H_

/* Genode includes */
#include <cpu_session/connection.h>

/* local includes */
#include <spy_root_component.h>
#include <rm_session/component.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Root component of an eavesdropping RM service
	 */
	class Rm_root : public Spy_root_component<Rm_session_component>
	{
		public:

			/**
			 * Constructor
			 *
			 * \param ep        entrypoint for 'Root'
			 * \param md_alloc  metadata allocator for 'Root'
			 */
			Rm_root(Rpc_entrypoint * const ep, Allocator * const md_alloc)
			: Spy_root_component(ep, md_alloc) { }
	};
}

#endif /* _INCLUDE__RM_SESSION__ROOT_H_ */

