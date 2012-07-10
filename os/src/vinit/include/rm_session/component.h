/*
 * \brief  Session component of an eavesdropping RM service
 * \author Martin Stein
 * \date   2012-06-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__COMPONENT_H_
#define _INCLUDE__RM_SESSION__COMPONENT_H_

/* Genode includes */
#include <base/env.h>

/* local includes */
#include <cpu_client.h>
#include <util/indexed.h>
#include <instruction.h>

namespace Init
{
	using namespace Genode;

	enum { MAX_RM_CLIENTS = 1024 };

	class Rm_session_component;

	/**
	 * Holds informations about a client of an eavesdropping RM service
	 */
	class Rm_client : public Indexed<Rm_client, MAX_RM_CLIENTS>
	{
		public:

			/**
			 * State of a load/store fault caused by a RM client
			 */
			struct State : Thread_state
			{
				unsigned reg; /* source/target register of
				               * the load/store instruction */
				Rm_session_component * rm_session; /* session on wich the fault
				                                    * had happened */
			};

		private:

			Rm_session_component * const _session; /* related RM session */
			Thread_capability const _thread; /* cap of related thread */
			State * _state; /* client state at its last load/store fault */

		public:

			/**
			 * Constructor
			 *
			 * \param  session  related local RM session
			 * \param  thread   related thread
			 */
			Rm_client(Rm_session_component * const session,
			          Thread_capability const thread)
			: _session(session), _thread(thread), _state(0) { }

			/***************
			 ** Accessors **
			 ***************/

			State * state() const { return _state; }
			void state(State * const state) { _state = state; }
			Thread_capability thread() const { return _thread; }
			Rm_session_component * session() const { return _session; }
	};

	/**
	 * Holds informations about a eavesdropped managed dataspace
	 */
	class Managed_dataspace : public Capability_indexed<Managed_dataspace>
	{
		Rm_session_component * const _sub_rm; /* RM that manages dataspace */

		public:

			/**
			 * Constructor
			 *
			 * \param  ds_cap  identifying capability
			 * \param  sub_rm  see same-named member
			 */
			Managed_dataspace(Dataspace_capability ds_cap,
			                  Rm_session_component * const sub_rm)
			:
				Capability_indexed(ds_cap),
				_sub_rm(sub_rm)
			{ }

			/***************
			 ** Accessors **
			 ***************/

			Rm_session_component * sub_rm() const { return _sub_rm; }
	};

	/**
	 * Session component of an eavesdropping RM service
	 */
	class Rm_session_component : public Rpc_object<Rm_session>
	{
		/**
		 * Holds informations about a RM attachment
		 */
		class Region : public Avl_node<Region>
		{
			void * const _begin; /* region base in the the related RM space */
			void * const _end; /* region top in the related RM space */
			Dataspace_capability const _ds_cap; /* backing-store dataspace */
			off_t const _offset; /* offset of the region within the
			                      * '_ds_cap' dataspace */

			public:

				/**
				 * Constructor
				 *
				 * For parameter description see same-named members.
				 */
				Region(void * const begin, void * const end,
				       Dataspace_capability const ds_cap, off_t const offset)
				:
					_begin(begin), _end(end),
					_ds_cap(ds_cap), _offset(offset)
				{ }

				/**
				 * Lookup region within this AVL subtree that covers 'addr'
				 */
				Region * find_by_addr(void * addr)
				{
					if ((addr >= _begin) && (addr < _end)) return this;
					Region * region = child(addr > _begin);
					return region ? region->find_by_addr(addr) : 0;
				}

				/***************
				 ** Accessors **
				 ***************/

				void * begin() const { return _begin; }
				off_t offset() const { return _offset; }
				Dataspace_capability ds_cap() const { return _ds_cap; }

				/**************
				 ** Avl_node **
				 **************/

				bool higher(Region * e) { return e->_begin > _begin; }
		};

		Rm_session_client _parent_rm; /* host session */
		Allocator * const _md_alloc; /* metadata allocator*/
		Avl_tree<Region> _region_map; /* remembers all attachments that
		                               * were made through this RM */
		Lock _region_map_lock; /* sync access to '_region_map' */

		/**
		 * Find RM attachment by address
		 *
		 * \param   local_addr  target address within this RM space
		 * \param   offset      holds offset of 'local_addr' within the found
		 *                      region if this returns not 0
		 * \return              requested region if it exists, otherwise 0
		 */
		Region * _find_region(void * local_addr, addr_t * offset)
		{
			/* lookup attachment that covers the address */
			Lock::Guard lock_guard(_region_map_lock);
			Region *first = _region_map.first();
			Region *region = first ? first->find_by_addr(local_addr) : 0;
			if (!region) return 0;

			/* check if the attachment is a managed dataspace */
			*offset = ((addr_t)local_addr - (addr_t)region->begin());
			Managed_dataspace * const managed_ds =
				Managed_dataspace::by_cap(region->ds_cap());
			if (managed_ds)
			{
				/* traverse into the sub RM that manages the dataspace */
				Rm_session_component * const sub_rm = managed_ds->sub_rm();
				region = sub_rm->_find_region((void *)*offset, offset);
			}
			return region;
		}

		public:

			/**
			 * Constructor
			 *
			 * \param  args  session arguments
			 * \param  md    session-metadata allocator
			 */
			Rm_session_component(const char * args, Allocator * md) :
				_parent_rm(env()->parent()->session<Rm_session>(args)),
				_md_alloc(md)
			{ }

			/*****************
			 ** Vrm_session **
			 *****************/

			State state()
			{
				/* get the classic RM state */
				Rm_session::State state = _parent_rm.state();

				/* get related RM client through the RM-state imprint */
				Rm_client * const rm_client = Rm_client::by_id(state.imprint);

				/* get the client state of the RM client */
				Thread_capability thread_cap = rm_client->thread();
				Cpu_client * const cpu_client = Cpu_client::by_cap(thread_cap);
				assert(cpu_client);
				Cpu_session * const cpu_session = cpu_client->session();
				assert(!rm_client->state());
				Rm_client::State * const client_state =
					new (_md_alloc) Rm_client::State;
				client_state->rm_session = this;
				rm_client->state(client_state);
				*static_cast<Thread_state *>(client_state) =
					cpu_session->state(thread_cap);

				/* find dataspace that covers the IP of the RM client */
				addr_t const ip = client_state->instruction_ptr();
				addr_t off;
				Rm_session_component * const rm = rm_client->session();
				Region * const region = rm->_find_region((void *)ip, &off);
				Dataspace_capability ds_cap = region->ds_cap();

				/* get the current instruction of the RM client */
				void * local = env()->rm_session()->attach(ds_cap, 0, region->offset());
				unsigned instr = *(unsigned *)((addr_t)local + off);
				env()->rm_session()->detach(local);

				/* decode the instruction and update states accordingly */
				bool writes;
				assert(Instruction::load_store(instr, writes, state.format,
				                               client_state->reg));
				if (writes)
				{
					/* instruction attempts to write, get the value */
					assert(state.type == Rm_session::WRITE_FAULT);
					assert(client_state->get_gpr(client_state->reg,
					                             state.value));
				} else assert(state.type == Rm_session::READ_FAULT);
				return state;
			}

			void processed(State state)
			{
				/* get affected RM client and its state via state imprint */
				Rm_client * const rm_client = Rm_client::by_id(state.imprint);
				Rm_client::State * const client_state = rm_client->state();
				assert(client_state);
				assert(client_state->rm_session == this);

				/* if instruction attempted to read, write back the result */
				if (state.type == Rm_session::READ_FAULT) {
					unsigned const reg = client_state->reg;
					assert(client_state->set_gpr(reg, state.value));
				}

				/* increase IP of the client to the next instruction */
				client_state->instruction_ptr(client_state->instruction_ptr() +
				                              Instruction::size());

				/* apply the new client state to the related thread */
				Thread_capability thread = rm_client->thread();
				Cpu_client * const cpu_client = Cpu_client::by_cap(thread);
				assert(cpu_client);
				Thread_state * const thread_state =
					static_cast<Thread_state *>(client_state);
				cpu_client->session()->state(thread, *thread_state);

				/* forget fault state of the client */
				rm_client->state(0);
				destroy(_md_alloc, client_state);

				/* let client continue */
				cpu_client->session()->resume(thread);
			}

			/****************
			 ** Rm_session **
			 ****************/

			Local_addr attach(Dataspace_capability ds_cap, size_t size,
			                  off_t off, bool use_local_addr,
			                  Rm_session::Local_addr local_addr,
			                  bool executable)
			{
				/* let our parent do the attachment */
				void * const addr =
					_parent_rm.attach(ds_cap, size, off, use_local_addr,
					                  local_addr, executable);

				/* remember attributes of the attachment */
				void * const end = (void *)((addr_t)addr + size);
				Region * const region =
					new (_md_alloc) Region(addr, end, ds_cap, off);
				_region_map.insert(region);
				return addr;
			}

			void detach(Local_addr la) { _parent_rm.detach(la); }

			Pager_capability add_client(Thread_capability t,
			                                    unsigned)
			{
				/* remember the new client */
				Rm_client * const c = new (_md_alloc) Rm_client(this, t);
				return _parent_rm.add_client(t, c->id());
			}

			void fault_handler(Signal_context_capability handler)
			{ _parent_rm.fault_handler(handler); }

			Dataspace_capability dataspace()
			{
				/* get managed dataspace from parent */
				Dataspace_capability ds_cap = _parent_rm.dataspace();
				if(!ds_cap.valid()) return ds_cap;

				/* if it is valid, remember the managed dataspace */
				new (_md_alloc) Managed_dataspace(ds_cap, this);
				return ds_cap;
			}
	};
}

#endif /* _INCLUDE__RM_SESSION__COMPONENT_H_ */

