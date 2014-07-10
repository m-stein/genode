/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__CORTEX_A9__PIC_BASE_H_
#define _SPEC__CORTEX_A9__PIC_BASE_H_

/* core includes */
#include <pic/arm_gic.h>
#include <processor_driver.h>

namespace Cortex_a9
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic_base : public Arm_gic::Pic
	{
		private:

			typedef Processor_driver Cpu;

		public:

			/**
			 * Constructor
			 */
			Pic_base() : Arm_gic::Pic(Cpu::PL390_DISTRIBUTOR_MMIO_BASE,
			                          Cpu::PL390_CPU_MMIO_BASE) { }
	};
}

#endif /* _SPEC__CORTEX_A9__PIC_BASE_H_ */
