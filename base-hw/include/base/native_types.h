/*
 * \brief  Platform specific basic Genode types
 * \author Martin Stein
 * \date   2012-01-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__INCLUDE__BASE__NATIVE_TYPES_H_
#define _BASE_HW__INCLUDE__BASE__NATIVE_TYPES_H_

/* Genode includes */
#include <kernel/syscalls.h>
#include <base/native_capability.h>

namespace Genode
{
	class Platform_thread;

	typedef int volatile Native_lock;
	typedef Platform_thread * Native_thread;
	typedef unsigned long Native_thread_id;
	typedef int Native_connection_state;

	/* XXX needs to be MMU dependent XXX */
	enum { MIN_MAPPING_SIZE_LOG2 = 12 };

	/**
	 * Get kernel-object identifier of the current thread
	 */
	inline Native_thread_id thread_get_my_native_id() {
		return Kernel::current_thread_id(); }

	/**
	 * Get the thread ID, wich is handled as invalid by the kernel
	 */
	inline Native_thread_id thread_invalid_id() { return 0; }

	/**
	 * Describes a pagefault
	 */
	struct Pagefault
	{
		unsigned long thread_id; /* Thread ID of the faulter */
		Software_tlb * software_tlb; /* Translation buffer that the
		                                * faulter is assigned to */
		addr_t virt_ip; /* Virtual instruction pointer of the faulter */
		addr_t virt_address; /* Virtual fault address */
		bool write; /* Write access attempted at fault? */

		/**
		 * Placement new operator
		 */
		void * operator new (size_t, void * p) { return p; }

		/**
		 * Construct invalid pagefault
		 */
		Pagefault() : thread_id(0) { }

		/**
		 * Construct valid pagefault
		 */
		Pagefault(unsigned const tid, Software_tlb * const sw_tlb,
		          addr_t const vip, addr_t const va, bool const w) :
			thread_id(tid), software_tlb(sw_tlb), virt_ip(vip),
			virt_address(va), write(w)
		{ }

		/**
		 * Validation
		 */
		bool valid() const { return thread_id != 0; }
	};

	/**
	 * Describes a userland-thread-context region
	 */
	struct Native_utcb
	{
		/* UTCB payload */
		union {
			char bytes[1<<MIN_MAPPING_SIZE_LOG2];
			umword_t words[sizeof(bytes)/sizeof(umword_t)];
		};

		/**
		 * Get pointer to a specific word within the UTCB
		 */
		umword_t * word(unsigned long const index) { return &words[index]; }

		/**
		 * Get the base of the UTCB
		 */
		void * base() { return (void *)bytes; }

		/**
		 * Get the UTCB size
		 */
		unsigned long size() { return sizeof(bytes); }
	};

	struct Cap_dst_policy
	{
		typedef Native_thread_id Dst;
		static bool valid(Dst pt) { return pt != 0; }
		static Dst  invalid()     { return 0;       }
		static void copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src);
	};

	typedef Native_capability_tpl<Cap_dst_policy> Native_capability;

	/**
	 * A coherent address region
	 */
	struct Native_region
	{
		addr_t base;
		size_t size;
	};
}

#endif /* _BASE_HW__INCLUDE__BASE__NATIVE_TYPES_H_ */

