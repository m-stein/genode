/*
 * \brief  Board-driver base
 * \author Stefan Kalkowski
 * \date   2013-11-25
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXYNOS5__BOARD_BASE_H_
#define _EXYNOS5__BOARD_BASE_H_

namespace Genode
{
	/**
	 * Board-driver base
	 */
	class Exynos5;
}

class Genode::Exynos5
{
	public:

		enum {
			/* normal RAM */
			RAM_0_BASE = 0x40000000,
			RAM_0_SIZE = 0x80000000,

			/* device IO memory */
			MMIO_0_BASE = 0x10000000,
			MMIO_0_SIZE = 0x10000000,

			/* interrupt controller */
			GIC_CPU_MMIO_BASE = 0x10480000,
			GIC_CPU_MMIO_SIZE = 0x00010000,

			/* UART */
			UART_2_MMIO_BASE = 0x12C20000,
			UART_2_IRQ       = 85,

			/* clock management unit */
			CMU_MMIO_BASE = 0x10010000,
			CMU_MMIO_SIZE = 0x24000,

			/* power management unit */
			PMU_MMIO_BASE = 0x10040000,
			PMU_MMIO_SIZE = 0x5000,

			/* USB */
			USB_HOST20_IRQ = 103,
			USB_DRD30_IRQ  = 104,

			/* pulse-width-modulation timer  */
			PWM_MMIO_BASE = 0x12dd0000,
			PWM_MMIO_SIZE = 0x1000,
			PWM_CLOCK     = 66000000,
			PWM_IRQ_0     = 68,

			/* multicore timer */
			MCT_MMIO_BASE = 0x101c0000,
			MCT_MMIO_SIZE = 0x1000,
			MCT_CLOCK     = 24000000,
			MCT_IRQ_L0    = 152,
			MCT_IRQ_L1    = 153,

			/* CPU cache */
			CACHE_LINE_SIZE_LOG2 = 6,

			/* IRAM */
			IRAM_BASE = 0x02020000,

			/* hardware name of the primary processor */
			PRIMARY_MPIDR_AFF_0 = 0,
		};
};

#endif /* _EXYNOS5__BOARD_BASE_H_ */
