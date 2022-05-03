/*
 * \brief  Linux Kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/init.h>
#include <lx_emul/time.h>

#include <linux/clockchips.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_clk.h>
#include <linux/of_fdt.h>


unsigned long long sched_clock(void)
{
	return lx_emul_time_counter() * 1000;
}


void time_init(void)
{
	/* arch/arm64/kernel/time.c */
	lx_emul_time_init(); /* replaces timer_probe() */
	tick_setup_hrtimer_broadcast();
	lpj_fine = 1000000 / HZ;
}


#include <asm/pgtable.h>

unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];

void lx_emul_setup_arch(void *dtb)
{
	/* fill zero page */
	memset(empty_zero_page, 0, PAGE_SIZE);

	/* calls from setup_arch of arch/arm64/kernel/setup.c */
	early_init_dt_scan(dtb);
	unflatten_device_tree();
}
