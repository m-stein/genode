/*
 * \brief  Dummy definitions for native types used for compiling unit tests
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <kernel/types.h>
#include <base/native_capability.h>
#include <base/stdint.h>

namespace Genode {

	typedef Kernel::Thread_id     Native_thread_id;
	typedef Native_thread_id      Native_thread;

	typedef Kernel::Protection_id Native_process_id;

	typedef Kernel::Utcb_unaligned                     Native_utcb;
	typedef Kernel::Paging::Physical_page::Permissions Native_page_permission;
	typedef Kernel::Paging::Physical_page::size_t      Native_page_size;

	Native_thread_id my_thread_id();

	struct Cap_dst_policy
	{
		typedef Kernel::Thread_id Dst;
		static bool valid(Dst tid) {
			return tid != Kernel::INVALID_THREAD_ID; }
		static Dst invalid()
			{ return Kernel::INVALID_THREAD_ID; }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
	typedef int Native_connection_state;

	struct Native_config
	{
		enum {
			CONTEXT_AREA_VIRTUAL_BASE = 0x40000000,
			CONTEXT_AREA_VIRTUAL_SIZE = 0x10000000,
			CONTEXT_VIRTUAL_SIZE = 0x00100000,
		};

		/**
		 * Thread-context area configuration.
		 */
		static addr_t context_area_virtual_base() { return CONTEXT_AREA_VIRTUAL_BASE; }
		static addr_t context_area_virtual_size() { return CONTEXT_AREA_VIRTUAL_SIZE; }

		/**
		 * Size of virtual address region holding the context of one thread
		 */
		static addr_t context_virtual_size() { return CONTEXT_VIRTUAL_SIZE; }
	};

	struct Native_pd_args { };
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
