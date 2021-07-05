/*
 * \brief  Main object of the kernel
 * \author Martin Stein
 * \date   2021-07-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

namespace Kernel {

	void main_handle_kernel_entry();

	time_t main_read_idle_thread_execution_time(unsigned cpu_idx);
}
