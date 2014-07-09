/*
 * \brief  Timer for kernel
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

/* core includes */
#include <board.h>
#include <spec/exynos5/timer_base.h>

namespace Kernel
{
	/**
	 * Kernel timer
	 */
	class Timer : public Exynos5::Timer_base
	{
		public:

			/**
			 * Return kernel name of timer interrupt of a specific processor
			 *
			 * \param processor_id  kernel name of targeted processor
			 */
			static unsigned interrupt_id(unsigned)
			{
				return Genode::Board::MCT_IRQ_L0;
			}

			/**
			 * Constructor
			 */
			Timer() : Exynos5::Timer_base(Genode::Board::MCT_MMIO_BASE,
			                              Genode::Board::MCT_CLOCK) { }
	};
}

#endif /* _TIMER_H_ */
