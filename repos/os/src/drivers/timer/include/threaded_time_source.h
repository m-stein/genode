/*
 * \brief  Time source that uses an extra thread for timeout handling
 * \author Norman Feske
 * \author Martin Stein
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _THREADED_TIME_SOURCE_H_
#define _THREADED_TIME_SOURCE_H_

/* Genode inludes */
#include <base/entrypoint.h>
#include <base/rpc_client.h>
#include <os/time_source.h>

namespace Timer {

	enum { STACK_SIZE = 8 * 1024 * sizeof(Genode::addr_t) };

	class Threaded_time_source;
}


class Timer::Threaded_time_source : public Genode::Time_source,
                                    protected Genode::Thread_deprecated<STACK_SIZE>
{
	private:

		struct Irq_dispatcher
		{
			GENODE_RPC(Rpc_do_dispatch, void, do_dispatch, Microseconds);
			GENODE_RPC_INTERFACE(Rpc_do_dispatch);
		};

		struct Irq_dispatcher_component : Genode::Rpc_object<Irq_dispatcher,
		                                                     Irq_dispatcher_component>
		{
				Timeout_handler *handler = nullptr;


				/********************
				 ** Irq_dispatcher **
				 ********************/

				void do_dispatch(Microseconds duration)
				{
					if (handler) {
						handler->handle_timeout(Microseconds(duration)); }
				}

		} _irq_dispatcher_component;

		Genode::Capability<Irq_dispatcher> _irq_dispatcher_cap;

		virtual void _wait_for_irq() = 0;


		/***********************
		 ** Thread_deprecated **
		 ***********************/

		void entry()
		{
			while (true) {
				_wait_for_irq();
				_irq_dispatcher_cap.call<Irq_dispatcher::Rpc_do_dispatch>(curr_time());
			}
		}

	public:

		Threaded_time_source(Genode::Entrypoint &ep)
		:
			Thread_deprecated<STACK_SIZE>("threaded_time_source"),
			_irq_dispatcher_cap(ep.rpc_ep().manage(&_irq_dispatcher_component))
		{ }

		void handler(Timeout_handler &handler) {
			_irq_dispatcher_component.handler = &handler; }
};

#endif /* _THREADED_TIME_SOURCE_H_ */
