/*
 * \brief  Server-sided implementation of a signal session
 * \author Martin stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__SIGNAL_SESSION_COMPONENT_H_

/* Genode includes */
#include <signal_session/signal_session.h>
#include <base/rpc_server.h>
#include <base/slab.h>
#include <base/allocator_guard.h>
#include <base/object_pool.h>

namespace Genode
{
	/**
	 * Maps a signal-receiver name to related core and kernel resources
	 */
	class Signal_session_receiver;

	/**
	 * Maps a signal-context name to related core and kernel resources
	 */
	class Signal_session_context;

	/**
	 * Server-sided implementation of a signal session
	 */
	class Signal_session_component;
}

class Genode::Signal_session_receiver
:
	public Object_pool<Signal_session_receiver>::Entry
{
	public:

		typedef Object_pool<Signal_session_receiver> Pool;

		/**
		 * Constructor
		 */
		Signal_session_receiver(Untyped_capability cap) : Entry(cap) { }

		/**
		 * Name of signal receiver
		 */
		unsigned id() const { return Pool::Entry::cap().dst(); }

		/**
		 * Size of SLAB block occupied by resources and this resource info
		 */
		static size_t slab_size()
		{
			return
				sizeof(Signal_session_receiver) +
				Kernel::signal_receiver_size();
		}

		/**
		 * Base of region donated to the kernel
		 */
		static addr_t kernel_donation(void * const slab_addr)
		{
			return (addr_t)slab_addr + sizeof(Signal_session_receiver);
		}
};

class Genode::Signal_session_context
:
	public Object_pool<Signal_session_context>::Entry
{
	public:

		typedef Object_pool<Signal_session_context> Pool;

		/**
		 * Constructor
		 */
		Signal_session_context(Untyped_capability cap) : Entry(cap) { }

		/**
		 * Name of signal context
		 */
		unsigned id() const { return Pool::Entry::cap().dst(); }

		/**
		 * Size of SLAB block occupied by resources and this resource info
		 */
		static size_t slab_size()
		{
			return
				sizeof(Signal_session_context) +
				Kernel::signal_context_size();
		}

		/**
		 * Base of region donated to the kernel
		 */
		static addr_t kernel_donation(void * const slab_addr)
		{
			return (addr_t)slab_addr + sizeof(Signal_session_context);
		}
};

class Genode::Signal_session_component : public Rpc_object<Signal_session>
{
	public:

		enum {
			/**
			 * Lastly Receiver::slab_size() was 112. Additionally we
			 * have to take in account, that the backing store might add
			 * its metadata and round up to next page size. So we choose
			 * 35 * 112 which mostly is save to end up in one page only.
			 */
			RECEIVERS_SB_SIZE = 3920,

			/**
			 * Lastly Context::slab_size() size was 124. Additionally we
			 * have to take in account, that the backing store might add
			 * its metadata and round up to next page size. So we choose
			 * 32 * 124 which mostly is save to end up in one page only.
			 */
			CONTEXTS_SB_SIZE  = 3968,
		};

	private:

		typedef Signal_session_receiver Receiver;
		typedef Signal_session_context  Context;

		Allocator_guard _md_alloc;
		Slab            _receivers_slab;
		Receiver::Pool  _receivers;
		Slab            _contexts_slab;
		Context::Pool   _contexts;
		char            _initial_receivers_sb [RECEIVERS_SB_SIZE];
		char            _initial_contexts_sb  [CONTEXTS_SB_SIZE];

		/**
		 * Destruct receiver 'r'
		 */
		void _destruct_receiver(Receiver * const r);

		/**
		 * Destruct context 'c'
		 */
		void _destruct_context(Context * const c);

	public:

		/**
		 * Constructor
		 *
		 * \param md         Metadata allocator
		 * \param ram_quota  Amount of RAM quota donated to this session
		 */
		Signal_session_component(Allocator * const md,
		                         size_t const ram_quota);

		/**
		 * Destructor
		 */
		~Signal_session_component();

		/**
		 * Raise the quota of this session by 'q'
		 */
		void upgrade_ram_quota(size_t const q) { _md_alloc.upgrade(q); }

		/******************************
		 ** Signal_session interface **
		 ******************************/

		Signal_receiver_capability alloc_receiver();

		Signal_context_capability
		alloc_context(Signal_receiver_capability, unsigned const);

		void free_receiver(Signal_receiver_capability);

		void free_context(Signal_context_capability);
};

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
