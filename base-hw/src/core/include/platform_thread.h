/*
 * \brief   Thread facility
 * \author  Martin Stein
 * \date    2012-02-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__CORE__INCLUDE__PLATFORM_THREAD_H_
#define _BASE_HW__SRC__CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <ram_session/ram_session.h>
#include <base/native_types.h>
#include <kernel/syscalls.h>
#include <kernel/log.h>

/* Core includes */
#include <assert.h>

namespace Genode {

	class Pager_object;
	class Thread_state;
	class Thread_base;
	class Rm_client;
	class Platform_thread;

	size_t kernel_thread_size();

	/**
	 * Core interface to Kernel_thread
	 */
	class Platform_thread
	{
		enum { NAME_MAX_LEN = 32 };

		Thread_base * _thread_base; /* Genode thread object we're belonging to */
		unsigned long _stack_size; /* Size of our stack */
		unsigned long _pd_id; /* ID of the kernel object of the PD we're assigned to */
		unsigned long _id; /* ID of our kernel object */
		Rm_client * _rm_client; /* A link to our RM session and to our pager object in one */
		bool _main_thread; /* Will we be the first thread to be executed in the PD */
		Native_utcb * _phys_utcb; /* Physical UTCB base */
		Native_utcb * _virt_utcb; /* Virtual UTCB base */
		Software_tlb * _software_tlb; /* TLB that this thread is assigned to after start */
		Ram_dataspace_capability _utcb; /* UTCB dataspace */
		char                     _name[NAME_MAX_LEN]; /* thread's name */

		/**
		 * Common construction part
		 */
		void _init();

		public:

			/**
			 * Constructor for core threads
			 */
			Platform_thread(const char * name,
			                bool const privileged,
			                Thread_base * const thread_base,
			                unsigned long const stack_size,
			                unsigned long const pd_id);

			/**
			 * Constructor for threads outside of core
			 */
			Platform_thread(const char * name, unsigned int priority,
			                addr_t utcb);

			/**
			 * Join PD identified by 'pd_id'
			 *
			 * \param   pd_id        ID of targeted PD
			 * \param   main_thread  Are we the main thread in this PD?
			 * \return               0 on success, < 0 otherwise
			 */
			int join_pd(unsigned long const pd_id,
			            bool const main_thread);

			/**
			 * Run this thread
			 */
			int start(void * ip, void * sp, unsigned int cpu_no = 0);

			void pause() { Kernel::pause_thread(_id); }
			void resume() { Kernel::resume_thread(_id); }

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking()
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			};

			/**
			 * Request thread state
			 *
			 * \param  state_dst  destination state buffer
			 *
			 * \retval  0 successful
			 * \retval -1 thread state not accessible
			 */
			int state(Genode::Thread_state *state_dst)
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
				return -1;
			};

			/**
			 * Destructor
			 */
			~Platform_thread()
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			}

			/**
			 * Return unique identification of this thread as faulter
			 */
			unsigned long pager_object_badge()
			{
				return _id;
			}

			/**
			 * Set the executing CPU for this thread.
			 */
			void set_cpu(unsigned int cpu_no)
			{
				kernel_log() << __PRETTY_FUNCTION__ << ": Not implemented\n";
				while (1) ;
			};

			/**
			 * Get thread name
			 */
			inline char const * name() const
			{
				return _name;
			}


			/***************
			 ** Accessors **
			 ***************/

			void pager(Pager_object * const pager);

			Pager_object * pager() const;

			unsigned long pd_id() const { return _pd_id; }

			Native_thread_id id() const { return _id; }

			unsigned long stack_size() const { return _stack_size; }

			Thread_base * thread_base()
			{
				if (!_thread_base) assert(_main_thread);
				return _thread_base;
			}

			Native_utcb * phys_utcb() const { return _phys_utcb; }

			Native_utcb * virt_utcb() const { return _virt_utcb; }

			Ram_dataspace_capability utcb() const { return _utcb; }

			Software_tlb * software_tlb() const { return _software_tlb; }
	};
}

#endif /* _BASE_HW__SRC__CORE__INCLUDE__PLATFORM_THREAD_H_ */

