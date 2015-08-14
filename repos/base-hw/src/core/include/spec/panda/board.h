/*
 * \brief  Board driver for core on pandaboard
 * \author Stefan Kalkowski
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <util/mmio.h>
#include <spec/cortex_a9/board_support.h>

namespace Kernel { class Pd; }

namespace Genode
{
	struct Board : Cortex_a9::Board
	{
		static void outer_cache_invalidate();
		static void outer_cache_flush();
		static void prepare_kernel();
		static void secondary_cpus_ip(void * const ip);
		static bool is_smp() { return true; }
	};

	void board_init_mp_async(bool const primary, Kernel::Pd * const core_pd);
	void board_init_mp_sync(unsigned const cpu);
}

#endif /* _BOARD_H_ */
