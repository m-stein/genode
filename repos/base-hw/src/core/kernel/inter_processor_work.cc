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

/* base-hw Core includes */
#include <kernel/inter_processor_work.h>
#include <kernel/thread.h>

using namespace Kernel;
using namespace Genode;


void Kernel::Inter_processor_work::execute()
{
	switch (_type) {
	case Type::THREAD_DESTRUCTION:
		_thread_destruction->execute();
		break;
	case Type::TLB_INVALIDATION:
		_tlb_invalidation->execute();
		break;
	}
	_nr_of_pending_cpus--;
	if (_nr_of_pending_cpus == 0) {
		_work_list.remove(&_le);
		_caller._restart();
	}
}


Kernel::Inter_processor_work::
Inter_processor_work(Inter_processor_work_list &remote_work_list,
                     Thread                    &caller,
                     Kernel_object<Thread>     &thread_to_destroy)
:
	_type               { Type::THREAD_DESTRUCTION },
	_nr_of_pending_cpus { 1 },
	_caller             { caller },
	_work_list          { remote_work_list }
{
	_thread_destruction.construct(thread_to_destroy);
	_work_list.insert(&_le);
}


Kernel::Inter_processor_work::
Inter_processor_work(Inter_processor_work_list &global_work_list,
                     Thread                    &caller,
                     Pd                        &pd,
                     addr_t                     addr,
                     size_t                     size,
                     unsigned                   nr_of_pending_cpus)
:
	_type               { Type::TLB_INVALIDATION },
	_nr_of_pending_cpus { nr_of_pending_cpus },
	_caller             { caller },
	_work_list          { global_work_list }
{
	_tlb_invalidation.construct(pd, addr, size);
	_work_list.insert(&_le);
}


void Kernel::Thread_destruction::execute()
{
	_thread_to_destroy.destruct();
}


Kernel::Tlb_invalidation::Tlb_invalidation(Pd     &pd,
                                           addr_t  addr,
                                           size_t  size)
:
	_pd   { pd },
	_addr { addr },
	_size { size }
{ }


Kernel::Thread_destruction::
Thread_destruction(Kernel_object<Thread> &thread_to_destroy)
:
	_thread_to_destroy { thread_to_destroy }
{ }
