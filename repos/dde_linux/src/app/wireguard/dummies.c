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


#include <linux/irq.h>
#include <linux/irqdesc.h>

int generic_handle_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6_stubs.h>

const struct ipv6_stub *ipv6_stub = NULL;


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/rcutiny.h>

void rcu_barrier(void)
{
	lx_emul_trace_and_stop(__func__);
}


void lx_user_init(void) {}
void lx_emul_associate_page_selftest(void) {}
