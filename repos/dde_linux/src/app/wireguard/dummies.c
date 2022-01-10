/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <asm/irq_regs.h>
struct pt_regs * __irq_regs = NULL;


#include <asm/preempt.h>

int __preempt_count = 0;


#include <linux/bitops.h>

unsigned long __sw_hweight64(__u64 w)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int __init early_irq_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/irq.h>
#include <linux/irqdesc.h>

int generic_handle_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/hrtimer.h>

void __init hrtimers_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/timer.h>

void __init init_timers(void)
{
	lx_emul_trace(__func__);
}


#include <net/ipv6_stubs.h>

const struct ipv6_stub *ipv6_stub = NULL;


#include <linux/radix-tree.h>

void __init radix_tree_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/rcutiny.h>

void rcu_barrier(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/clock.h>

void __init sched_clock_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/interrupt.h>

void __init softirq_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/tick.h>

void __init tick_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/timekeeping.h>

void __init timekeeping_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/workqueue.h>

void __init workqueue_init_early(void)
{
	lx_emul_trace(__func__);
}


