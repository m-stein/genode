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

#include <asm/cpufeature.h>

bool cpu_have_feature(unsigned int num)
{
	return 0;
}

#include <lx_emul.h>

u64 vabits_actual;

void rcu_read_unlock_strict(void)
{
	lx_emul_trace(__func__);
}

unsigned long __must_check __arch_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}








#include <linux/of_fdt.h>

void __init unflatten_device_tree(void)
{
	lx_emul_trace(__func__);
}

#include <linux/of_fdt.h>

bool __init early_init_dt_scan(void * params)
{
	lx_emul_trace(__func__);
	return false;
}

void __init irqchip_init(void) {
	lx_emul_trace(__func__);
}









#include <linux/sched_clock.h>

void __init sched_clock_register(u64 (* read)(void),int bits,unsigned long rate)
{
	lx_emul_trace(__func__);
}

#include <linux/of_clk.h>

void __init of_clk_init(const struct of_device_id * matches)
{
	lx_emul_trace(__func__);
}


#include <linux/sched_clock.h>

void __init generic_sched_clock_init(void)
{
	lx_emul_trace(__func__);
}

#include <linux/sched.h>

void do_set_cpus_allowed(struct task_struct * p,const struct cpumask * new_mask)
{
	lx_emul_trace(__func__);
}


#include <linux/of.h>

void __init of_core_init(void)
{
	lx_emul_trace(__func__);
}

#include <linux/stop_machine.h>

int stop_machine(cpu_stop_fn_t fn,void * data,const struct cpumask * cpus)
{
	return (*fn)(data);
}








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


#include <linux/kernel_stat.h>

void account_process_tick(struct task_struct * p,int user_tick)
{
	lx_emul_trace(__func__);
}


#include <linux/random.h>

int add_random_ready_callback(struct random_ready_callback * rdy)
{
	lx_emul_trace(__func__);
	return 0;
}




extern int __init buses_init(void);
int __init buses_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sched/loadavg.h>

void calc_global_load(void)
{
	lx_emul_trace(__func__);
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


#include <linux/tracepoint-defs.h>

const struct trace_print_flags gfpflag_names[]  = { {0,NULL}};


#include <linux/sched/signal.h>

void ignore_signals(struct task_struct * t)
{
	lx_emul_trace(__func__);
}


#include <net/ipv6_stubs.h>

const struct ipv6_stub *ipv6_stub = NULL;


#include <linux/prandom.h>

unsigned long net_rand_noise;


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

void rcu_barrier(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcupdate.h>

void rcu_sched_clock_irq(int user)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

int register_netdevice(struct net_device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <net/net_namespace.h>

int register_pernet_device(struct pernet_operations * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <net/sock.h>

void sk_set_memalloc(struct sock * sk)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void synchronize_net(void)
{
	lx_emul_trace(__func__);
}


#include <linux/timekeeper_internal.h>

void update_vsyscall(struct timekeeper * tk)
{
	lx_emul_trace(__func__);
}


#include <linux/tracepoint-defs.h>

const struct trace_print_flags vmaflag_names[]  = { {0,NULL}};


#include <linux/rtnetlink.h>

void rtnl_lock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_unlock(void)
{
	lx_emul_trace(__func__);
}


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace(__func__);
}


#include <linux/netdevice.h>

void napi_enable(struct napi_struct * n)
{
	lx_emul_trace(__func__);
}


#include <linux/mmzone.h>

struct mem_section ** mem_section = NULL;


unsigned long phys_base = 0;


#include <asm/atomic.h>

DEFINE_STATIC_KEY_FALSE(bpf_stats_enabled_key);


#include <linux/net.h>

int net_ratelimit(void)
{
	lx_emul_trace(__func__);
	return 0;
}


__wsum csum_partial(const void * buff,int len,__wsum sum)
{
	lx_emul_trace_and_stop(__func__);
}

#include <linux/rcutree.h>

void kvfree(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}
