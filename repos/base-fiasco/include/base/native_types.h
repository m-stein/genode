/*
 * \brief  Native types on L4/Fiasco
 * \author Norman Feske
 * \date   2008-07-26
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <base/native_capability.h>
#include <base/stdint.h>

namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Platform_thread;

	typedef Fiasco::l4_threadid_t Native_thread_id;

	struct Cap_dst_policy
	{
		typedef Fiasco::l4_threadid_t Dst;
		static bool valid(Dst id) { return !Fiasco::l4_is_invalid_id(id); }
		static Dst invalid()
		{
			using namespace Fiasco;
			return L4_INVALID_ID;
		}
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	struct Native_thread
	{
		Native_thread_id l4id;

		/**
		 * Only used in core
		 *
		 * For 'Thread' objects created within core, 'pt' points to
		 * the physical thread object, which is going to be destroyed
		 * on destruction of the 'Thread'.
		 */
		Platform_thread *pt;
	};

	inline unsigned long convert_native_thread_id_to_badge(Native_thread_id tid)
	{
		/*
		 * Fiasco has no server-defined badges for page-fault messages.
		 * Therefore, we have to interpret the sender ID as badge.
		 */
		return tid.raw;
	}

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;
	typedef Fiasco::l4_threadid_t Native_connection_state;
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
