/*
 * \brief   Kernel interface for inter-processor communication
 * \author  Martin Stein
 * \date    2021-08-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KERNEL__INTER_PROCESSOR_WORK_H_
#define _KERNEL__INTER_PROCESSOR_WORK_H_

/* base-hw Core includes */
#include <kernel/ada_interfacing.h>

namespace Kernel {

	struct Inter_processor_work;
	struct Inter_processor_work_list;
}


struct Kernel::Inter_processor_work_list : Opaque_ada_type<Inter_processor_work_list, 16>
{
	Inter_processor_work_list();

	void execute_each();
};


struct Kernel::Inter_processor_work : Opaque_ada_type<Inter_processor_work, 64>
{
	void execute();

	Inter_processor_work(Inter_processor_work_list &global_work_list,
	                     Thread                    &caller,
	                     unsigned                   nr_of_cpus);

	Inter_processor_work(Inter_processor_work_list     &remote_work_list,
	                     Thread                        &caller,
	                     Genode::Kernel_object<Thread> &thread_to_destroy);
};

#endif /* _KERNEL__INTER_PROCESSOR_WORK_H_ */
