/*
 * \brief  Root component template for eavesdropping services
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SPY_ROOT_COMPONENT_H_
#define _INCLUDE__SPY_ROOT_COMPONENT_H_

/* Genode includes */
#include <base/allocator.h>
#include <root/component.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Root component template for eavesdropping services
	 */
	template <typename _SESSION_COMPONENT>
	class Spy_root_component : public Root_component<_SESSION_COMPONENT>
	{
		typedef _SESSION_COMPONENT Session_component;

		Allocator * _md_alloc; /* metadata allocator */

		protected:

			/******************************
			 ** Root_component interface **
			 ******************************/

			Session_component * _create_session(const char * args)
			{
				Session_component * const session =
					new (_md_alloc) Session_component(args, _md_alloc);
				return session;
			}

		public:

			/**
			 * Constructor
			 *
			 * \param ep  entrypoint for 'Root_component'
			 * \param md  metadata allocator for session creation
			 */
			Spy_root_component(Rpc_entrypoint * const ep, Allocator * const md)
			: Root_component<Session_component>(ep, md), _md_alloc(md) { }
	};
}

#endif /* _INCLUDE__SPY_ROOT_COMPONENT_H_ */

