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


#include <linux/cpuhotplug.h>

int __cpuhp_setup_state(enum cpuhp_state state,const char * name,bool invoke,int (* startup)(unsigned int cpu),int (* teardown)(unsigned int cpu),bool multi_instance)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <asm/irq_regs.h>
struct pt_regs * __irq_regs = NULL;


#include <asm/preempt.h>

int __preempt_count = 0;


#include <linux/bitops.h>

unsigned long __sw_hweight64(__u64 w)
{
	lx_emul_trace_and_stop(__func__);
}


struct cpuinfo_x86 boot_cpu_data __read_mostly;


extern int __init buses_init(void);
int __init buses_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init classes_init(void);
int __init classes_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init devices_init(void);
int __init devices_init(void)
{
	lx_emul_trace(__func__);
	return 0;
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


#include <net/genetlink.h>

int genl_register_family(struct genl_family * family)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/tracepoint-defs.h>

const struct trace_print_flags gfpflag_names[]  = { {0,NULL}};


#include <linux/hrtimer.h>

void __init hrtimers_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/sched/signal.h>

void ignore_signals(struct task_struct * t)
{
	lx_emul_trace(__func__);
}


#include <linux/timer.h>

void init_timer_key(struct timer_list * timer,void (* func)(struct timer_list *),unsigned int flags,const char * name,struct lock_class_key * key)
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


#include <linux/tracepoint-defs.h>

const struct trace_print_flags pageflag_names[] = { {0,NULL}};


extern int __init platform_bus_init(void);
int __init platform_bus_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/rcutiny.h>

void rcu_barrier(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/net_namespace.h>

int register_pernet_device(struct pernet_operations * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sched/clock.h>

void __init sched_clock_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/sched.h>

signed long __sched schedule_timeout(signed long timeout)
{
	lx_emul_trace(__func__);
	return timeout;
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


#include <linux/tracepoint-defs.h>

const struct trace_print_flags vmaflag_names[]  = { {0,NULL}};
