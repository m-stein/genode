/*
 * \brief  Delivery and reception of asynchronous notifications on HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__INCLUDE__BASE__SIGNAL_H__
#define _BASE_HW__INCLUDE__BASE__SIGNAL_H__

/* Genode includes */
#include <signal_session/signal_session.h>
#include <base/lock.h>
#include <util/list.h>

namespace Genode
{
	/**
	 * Represents a bunch of asynchronuosly triggered events
	 * that target the same context
	 *
	 * Because a signal can trigger asynchronously at a context,
	 * the kernel accumulates them and provides them as such a bunch,
	 * once the receiver indicates that he is ready to receive.
	 */
	class Signal
	{
		unsigned long _imprint; /* Receiver-local signal-context pointer */
		int _num; /* How often this signal has been triggered */

		public:

			/**
			 * Construct valid signal
			 */
			Signal(unsigned long const imprint, int const num) :
				_imprint(imprint), _num(num)
			{ }

			/***************
			 ** Accessors **
			 ***************/

			Signal_context * context() { return (Signal_context *)_imprint; }
			int num() { return _num; }
	};

	/**
	 * A specific signal type that a transmitter can target when it submits
	 *
	 * One receiver might handle multiple signal contexts,
	 * but a signal context is owned by exactly one signal receiver.
	 */
	typedef List<Signal_context> Context_list;
	class Signal_context : public Context_list::Element
	{
		friend class Signal_receiver;

		Signal_receiver * _receiver; /* Receiver that manages us */
		Signal_context_capability _cap; /* Holds the name of our context
		                                 * kernel-object as 'dst' */
		Lock _lock; /* Serialize access to this context */

		public:

			/**
			 * Construct context that is not yet managed by a receiver
			 */
			Signal_context() : _receiver(0), _lock(Lock::UNLOCKED) { }

			/**
			 * Destructor
			 */
			virtual ~Signal_context() { }

			/* Only needed to enable one to type a capability with us */
			GENODE_RPC_INTERFACE();
	};


	/**
	 * To submit signals to one specific context
	 *
	 * Multiple transmitters can submit to the same context
	 */
	class Signal_transmitter
	{
		/* Names the targeted context kernel-object with its 'dst' field */
		Signal_context_capability _context;

		public:

			/**
			 * Constructor
			 */
			Signal_transmitter(Signal_context_capability const c =
			                   Signal_context_capability()) :
				_context(c)
			{ }

			/**
			 * Trigger a signal at the context we target 'num' times
			 */
			void submit(int const num = 1) {
				Kernel::submit_signal(_context.dst(), num); }

			/***************
			 ** Accessors **
			 ***************/

			void context(Signal_context_capability const c) { _context = c; }
	};


	/**
	 * Manage multiple signal contexts and receive signals targeted to them
	 */
	class Signal_receiver
	{
			Context_list _contexts; /* Contexts that we manage */
			Lock _contexts_lock; /* Serialize access to 'contexts' */
			Signal_receiver_capability _cap; /* Holds name of our receiver
			                                  * kernel-object as 'dst' */

			/**
			 * Let a context no longer be managed by us
			 *
			 * Doesn't serialize any member access.
			 */
			void _unsync_dissolve(Signal_context * const c);

		public:

			/* Exceptions */
			class Context_already_in_use : public Exception { };
			class Context_not_associated : public Exception { };

			/**
			 * Constructor
			 */
			Signal_receiver();

			/**
			 * Destructor
			 */
			~Signal_receiver();

			/**
			 * Let a context be managed by us
			 *
			 * \return  Capability wich names the context kernel-object in its
			 *          'dst' field . Can be used as target for transmitters
			 */
			Signal_context_capability manage(Signal_context * const c);

			/**
			 * Let a context no longer be managed by us
			 */
			void dissolve(Signal_context * const c);

			/**
			 * Block on any signal that is triggered at one of our contexts
			 */
			Signal wait_for_signal();
	};
}

#endif /* _BASE_HW__INCLUDE__BASE__SIGNAL_H__ */

