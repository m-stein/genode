/*
 * \brief  Provide informations about a client of an eavesdropping CPU service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_CLIENT_H_
#define _INCLUDE__CPU_CLIENT_H_

/* Genode includes */
#include <cpu_session/cpu_session.h>

/* local includes */
#include <util/indexed.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Provide informations about a client of an eavesdropping CPU service
	 */
	class Cpu_client : public Capability_indexed<Cpu_client>
	{
		Cpu_session * const _session; /* related CPU session */

		public:

			/**
			 * Constructor
			 *
			 * \param  thread_cap  capability of related thread
			 * \param  session     related CPU session
			 */
			Cpu_client(Thread_capability thread_cap,
			           Cpu_session * const session)
			:
				Capability_indexed(thread_cap), _session(session)
			{ }

			/***************
			 ** Accessors **
			 ***************/

			Cpu_session * session() const { return _session; }
	};
}

#endif /* _INCLUDE__CPU_CLIENT_H_ */

