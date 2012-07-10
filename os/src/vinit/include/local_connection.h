/*
 * \brief  Connection to a local service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__LOCAL_CONNECTION_H_
#define _INCLUDE__LOCAL_CONNECTION_H_

/* Genode includes */
#include <root/component.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Connection to a local service
	 *
	 * \param  _SESSION_COMPONENT  component type of the targeted session
	 */
	template <typename _SESSION_COMPONENT>
	class Local_connection
	{
		enum { FORMAT_STRING_SIZE = Parent::Session_args::MAX_SIZE };

		typedef _SESSION_COMPONENT Session_component;
		typedef typename Session_component::Rpc_interface Session;
		typedef Root_component<Session_component> Session_root;

		Session_root * const _root; /* related local root component */
		Capability<Session> _cap; /* capability of related session */

		public:

			/**
			 * Creates a new session
			 *
			 * \param  root         local root component to create session at
			 * \param  format_args  session arguments
			 */
			static Capability<Session> session(Session_root * const root,
			                                    const char * format_args, ...)
			{
				char buf[FORMAT_STRING_SIZE];
				va_list list;
				va_start(list, format_args);
				String_console sc(buf, FORMAT_STRING_SIZE);
				sc.vprintf(format_args, list);
				va_end(list);
				return reinterpret_cap_cast<Session>(root->session(buf));
			}

			/**
			 * Constructor
			 *
			 * \param  root  related local root component
			 * \param  cap   capability of related session
			 */
			Local_connection(Session_root * root, Capability<Session> cap)
			: _root(root), _cap(cap) { }

			/**
			 * Destructor
			 */
			~Local_connection() { _root->close(_cap); }

			/**
			 * Return session capability
			 */
			Capability<Session> cap() const { return _cap; }
	};
}

#endif /* _INCLUDE__LOCAL_CONNECTION_H_ */

