/*
 * \brief  Core-specific instance of the CPU session/thread interfaces
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/allocator_guard.h>
#include <base/lock.h>
#include <base/pager.h>
#include <base/rpc_server.h>
#include <cpu_session/cpu_session.h>

/* core includes */
#include <cpu_thread_allocator.h>
#include <platform_thread.h>
#include <trace/control_area.h>
#include <trace/source_registry.h>

namespace Genode {

	/**
	 * RPC interface of CPU thread
	 *
	 * We make 'Cpu_thread' a RPC object only to be able to lookup CPU threads
	 * from thread capabilities supplied as arguments to CPU-session functions.
	 * A CPU thread does not provide an actual RPC interface.
	 */
	struct Cpu_thread
	{
		GENODE_RPC_INTERFACE();
	};


	class Cpu_thread_component : public Rpc_object<Cpu_thread>,
	                             public List<Cpu_thread_component>::Element
	{
		public:

			typedef Trace::Session_label Session_label;
			typedef Trace::Thread_name   Thread_name;

		private:

			size_t              const _quota;
			Thread_name         const _name;
			Platform_thread           _platform_thread;
			bool                      _bound;            /* pd binding flag */
			Signal_context_capability _sigh;             /* exception handler */
			unsigned            const _trace_control_index;
			Trace::Source             _trace_source;

		public:

			Cpu_thread_component(size_t quota, Session_label const &label,
			                     Thread_name const &name,
			                     unsigned priority, addr_t utcb,
			                     Signal_context_capability sigh,
			                     unsigned trace_control_index,
			                     Trace::Control &trace_control)
			:
				_quota(quota), _name(name),
				_platform_thread(quota, name.string(), priority, utcb),
				_bound(false), _sigh(sigh),
				_trace_control_index(trace_control_index),
				_trace_source(label, _name, trace_control)
			{
				update_exception_sigh();
			}


			/************************
			 ** Accessor functions **
			 ************************/

			Platform_thread *platform_thread() { return &_platform_thread; }
			bool             bound()     const { return _bound; }
			void             bound(bool b)     { _bound = b; }
			Trace::Source   *trace_source()    { return &_trace_source; }
			size_t           quota() const     { return _quota; }

			void sigh(Signal_context_capability sigh)
			{
				_sigh = sigh;
				update_exception_sigh();
			}

			/**
			 * Propagate exception handler to platform thread
			 */
			void update_exception_sigh();

			/**
			 * Return index within the CPU-session's trace control area
			 */
			unsigned trace_control_index() const { return _trace_control_index; }
	};


	class Cpu_session_component : public Rpc_object<Cpu_session>,
	                              public List<Cpu_session_component>::Element
	{
		public:

			typedef Cpu_thread_component::Session_label Session_label;

		private:

			Session_label              _label;
			Rpc_entrypoint * const     _session_ep;
			Rpc_entrypoint            *_thread_ep;
			Pager_entrypoint          *_pager_ep;
			Allocator_guard            _md_alloc;          /* guarded meta-data allocator */
			Cpu_thread_allocator       _thread_alloc;      /* meta-data allocator */
			Lock                       _thread_alloc_lock; /* protect allocator access */
			List<Cpu_thread_component> _thread_list;
			Lock                       _thread_list_lock;  /* protect thread list */
			unsigned                   _priority;          /* priority of threads
			                                                  created with this
			                                                  session */
			Affinity::Location         _location;          /* CPU affinity of this 
			                                                  session */
			Trace::Source_registry    &_trace_sources;
			Trace::Control_area        _trace_control_area;

			/*
			 * Members for quota accounting
			 */

			Cpu_session_component *     _ref;
			size_t                      _used;
			size_t                      _quota;
			List<Cpu_session_component> _ref_members;
			Lock                        _ref_members_lock;

			/*
			 * Utilities for quota accounting
			 */

			size_t _avail() { return _quota - _used; }

			size_t _local_to_global(size_t const q) const {
				return (q * _quota) >> Cpu_session::QUOTA_LIMIT_LOG2; }

			size_t _global_to_local(size_t const q) const {
				if (!_quota) { return 0; }
				return (q << Cpu_session::QUOTA_LIMIT_LOG2) / _quota; }

			int _insuff_for_transfer(size_t const q);

			void _insuff_for_consume(size_t const q);

			int _transfer_back(size_t const q)
			{
				_quota -= q;
				_ref->_used -= q;
				return 0;
			}

			int _transfer_forth(Cpu_session_component * const s, size_t const q)
			{
				s->_quota += q;
				_used += q;
				return 0;
			}

			void _insert_ref_member(Cpu_session_component * const s)
			{
				Lock::Guard lock_guard(_ref_members_lock);
				_ref_members.insert(s);
				s->_ref = this;
			}

			void _unsync_remove_ref_member(Cpu_session_component * const s)
			{
				s->_ref = 0;
				_ref_members.remove(s);
			}

			void _remove_ref_member(Cpu_session_component * const s)
			{
				Lock::Guard lock_guard(_ref_members_lock);
				_unsync_remove_ref_member(s);
			}

			void _deinit_ref_account();

			void _deinit_threads();

			/**
			 * Exception handler that will be invoked unless overridden by a
			 * call of 'Cpu_session::exception_handler'.
			 */
			Signal_context_capability _default_exception_handler;

			/**
			 * Raw thread-killing functionality
			 *
			 * This function is called from the 'kill_thread' function and
			 * the destructor. Each these functions grab the list lock
			 * by themselves and call this function to perform the actual
			 * killing.
			 */
			void _unsynchronized_kill_thread(Cpu_thread_component *thread);

		public:

			/**
			 * Constructor
			 */
			Cpu_session_component(Rpc_entrypoint         *session_ep,
			                      Rpc_entrypoint         *thread_ep,
			                      Pager_entrypoint       *pager_ep,
			                      Allocator              *md_alloc,
			                      Trace::Source_registry &trace_sources,
			                      const char *args, Affinity const &affinity,
			                      size_t quota);

			/**
			 * Destructor
			 */
			~Cpu_session_component();

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }


			/***************************
			 ** CPU session interface **
			 ***************************/

			Thread_capability create_thread(size_t, Name const &, addr_t);
			Ram_dataspace_capability utcb(Thread_capability thread);
			void kill_thread(Thread_capability);
			int set_pager(Thread_capability, Pager_capability);
			int start(Thread_capability, addr_t, addr_t);
			void pause(Thread_capability thread_cap);
			void resume(Thread_capability thread_cap);
			void cancel_blocking(Thread_capability);
			int name(Thread_capability, char *, size_t);
			Thread_state state(Thread_capability);
			void state(Thread_capability, Thread_state const &);
			void exception_handler(Thread_capability, Signal_context_capability);
			Affinity::Space affinity_space() const;
			void affinity(Thread_capability, Affinity::Location);
			Dataspace_capability trace_control();
			unsigned trace_control_index(Thread_capability);
			Dataspace_capability trace_buffer(Thread_capability);
			Dataspace_capability trace_policy(Thread_capability);
			int ref_account(Cpu_session_capability c);
			int transfer_quota(Cpu_session_capability c, size_t q);
			size_t used();
			size_t quota();
	};
}

#endif /* _CORE__INCLUDE__CPU_SESSION_COMPONENT_H_ */
