/*
 * \brief  Session component of an eavesdropping CPU service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU_SESSION__COMPONENT_H_
#define _INCLUDE__CPU_SESSION__COMPONENT_H_

/* Genode includes */
#include <base/env.h>
#include <cpu_session/client.h>

/* local includes */
#include <cpu_client.h>

namespace Init
{
	using namespace Genode;

	/**
	 * Session component of an eavesdropping CPU service
	 */
	class Cpu_session_component : public Rpc_object<Cpu_session>
	{
		Cpu_session_client _parent_session; /* host session */
		Allocator * const _md_alloc; /* metadata allocator */

		public:

			/**
			 * Constructor
			 *
			 * \param  args  session arguments
			 * \param  md    metadata allocator
			 */
			Cpu_session_component(const char * args, Allocator * md) :
				_parent_session(env()->parent()->session<Cpu_session>(args)),
				_md_alloc(md)
			{ }

			/*****************
			 ** Cpu_session **
			 *****************/

			Thread_capability create_thread(Name const &name, addr_t utcb)
			{
				/* let parent create thread */
				Thread_capability thread_cap;
				thread_cap = _parent_session.create_thread(name, utcb);

				/* if creation failed do not remember this thread */
				if(!thread_cap.valid()) return thread_cap;

				/* remember the new thread and its attributes */
				new (_md_alloc) Cpu_client(thread_cap, &_parent_session);
				return thread_cap;
			}

			Ram_dataspace_capability utcb(Thread_capability thread)
			{ return _parent_session.utcb(thread); }

			void kill_thread(Thread_capability thread)
			{ _parent_session.kill_thread(thread); }

			Thread_capability first() { return _parent_session.first(); }

			Thread_capability next(Thread_capability curr)
			{ return _parent_session.next(curr); }

			int set_pager(Thread_capability thread, Pager_capability  pager)
			{ return _parent_session.set_pager(thread, pager); }

			int start(Thread_capability thread, addr_t ip, addr_t sp)
			{ return _parent_session.start(thread, ip, sp); }

			void pause(Thread_capability thread)
			{ return _parent_session.pause(thread); }

			void resume(Thread_capability thread)
			{ return _parent_session.resume(thread); }

			void cancel_blocking(Thread_capability thread)
			{ return _parent_session.cancel_blocking(thread); }

			Thread_state state(Thread_capability thread)
			{ return _parent_session.state(thread); }

			void state(Thread_capability thread, Thread_state state)
			{ _parent_session.state(thread, state); }

			void exception_handler(Thread_capability thread,
			                       Signal_context_capability handler)
			{ return _parent_session.exception_handler(thread, handler); }

			void single_step(Thread_capability thread, bool enable)
			{ return _parent_session.single_step(thread, enable); }
	};
}

#endif /* _INCLUDE__CPU_SESSION__COMPONENT_H_ */

