/*
 * \brief   Kernel interface for inter-processor communication
 * \author  Stefan Kalkowski
 * \author  Martin Stein
 * \date    2018-11-15
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__INTER_PROCESSOR_WORK_H_
#define _KERNEL__INTER_PROCESSOR_WORK_H_

/* base includes */
#include <util/list.h>
#include <base/stdint.h>

/* base-hw Core includes */
#include <object.h>

namespace Kernel {

	class Pd;

	class Tlb_invalidation;
	class Thread_destruction;
	class Inter_processor_work;

	using Inter_processor_work_list =
		Genode::List<Genode::List_element<Inter_processor_work> >;
}


class Kernel::Tlb_invalidation
{
	private:

		Pd                   &_pd;
		Genode::addr_t const  _addr;
		Genode::size_t const  _size;

	public:

		Tlb_invalidation(Pd             &pd,
		                 Genode::addr_t  addr,
		                 Genode::size_t  size);

		void execute();
};


class Kernel::Thread_destruction
{
	private:

		Genode::Kernel_object<Thread> &_thread_to_destroy;

	public:

		Thread_destruction(Genode::Kernel_object<Thread> &thread_to_destroy);

		void execute();
};


class Kernel::Inter_processor_work
{
	private:

		enum class Type { THREAD_DESTRUCTION, TLB_INVALIDATION };

		Type                                 const  _type;
		unsigned                                    _nr_of_pending_cpus;
		Thread                                     &_caller;
		Inter_processor_work_list                  &_work_list;
		Genode::List_element<Inter_processor_work>  _le                 { this };
		Genode::Constructible<Tlb_invalidation>     _tlb_invalidation   { };
		Genode::Constructible<Thread_destruction>   _thread_destruction { };

	public:

		void execute();

		/**
		 * Constructor for a TLB invalidation
		 */
		Inter_processor_work(Inter_processor_work_list &global_work_list,
		                     Thread                    &caller,
		                     Pd                        &pd,
		                     Genode::addr_t             addr,
		                     Genode::size_t             size,
		                     unsigned                   nr_of_cpus);

		/**
		 * Constructor for a thread destruction
		 */
		Inter_processor_work(Inter_processor_work_list     &remote_work_list,
		                     Thread                        &caller,
		                     Genode::Kernel_object<Thread> &thread_to_destroy);
};

#endif /* _KERNEL__INTER_PROCESSOR_WORK_H_ */
