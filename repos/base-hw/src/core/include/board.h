/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <drivers/board_base.h>

namespace Genode
{
	class Board : public Board_base
	{
		public:
			static bool is_smp() { return false; }
			static void init_mp_async(bool const primary, addr_t const core_tt,
			                          unsigned const core_id);
			static void init_mp_sync(unsigned const cpu);

			/*
			 * Dummies
			 */

			static void outer_cache_invalidate() { }
			static void outer_cache_flush() { }
			static void prepare_kernel() { }
			static void secondary_cpus_ip(void * const ip) { }
	};

	class L2_cache
	{
		public:

			/*
			 * Dummies
			 */

			void invalidate() { }
			void disable() { }
			void enable() { }
	};

	L2_cache * l2_cache();
}

#endif /* _BOARD_H_ */
