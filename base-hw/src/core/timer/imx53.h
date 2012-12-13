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

#ifndef _TIMER__IMX53_H_
#define _TIMER__IMX53_H_

/* core includes */
#include <board.h>
#include <drivers/timer/epit_base.h>

namespace Imx53
{
	/**
	 * Kernel timer
	 */
	class Timer : public Genode::Epit_base
	{
		public:

			enum { IRQ = Board::EPIT_1_IRQ };

			/**
			 * Constructor
			 */
			Timer() : Epit_base(Board::EPIT_1_MMIO_BASE) { }
	};
}

#endif /* _TIMER__IMX53_H_ */

