/*
 * \brief   Singlethreaded minimalistic kernel
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-09-30
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__KERNEL__KERNEL_H_
#define _CORE__KERNEL__KERNEL_H_


namespace Kernel {

	class Pd;

	Pd  &core_pd();
}


namespace Kernel {

	class Main;
}


class Kernel::Main
{
		void _handle_kernel_entry();

		static void _initialize_spunky();

	public:

		static void load_global_instance_and_handle_kernel_entry();

		static void construct_global_instance_and_handle_kernel_entry();
};

#endif /* _CORE__KERNEL__KERNEL_H_ */
