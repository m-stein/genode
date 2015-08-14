/*
 * \brief  Communictation with the Pandaboard Trustzone Hypervisor
 * \author Stefan Kalkowski
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pl310.h>
#include <cpu.h>

using namespace Genode;

enum Trustzone_hypervisor_syscalls
{
	CPU_ACTLR_SMP_BIT_RAISE =  0x25,
	PL310_DEBUG_WRITE       = 0x100,
	PL310_ENABLE_WRITE      = 0x102,
	PL310_AUX_WRITE         = 0x109,
};

static inline void trustzone_hypervisor_call(addr_t func, addr_t val)
{
	register addr_t _func asm("r12") = func;
	register addr_t _val  asm("r0")  = val;
	asm volatile("dsb; smc #0" :: "r" (_func), "r" (_val)
	             : "memory", "cc", "r1", "r2", "r3", "r4", "r5",
	               "r6", "r7", "r8", "r9", "r10", "r11");
}


void Genode::pl310_enable(bool const v) {
	trustzone_hypervisor_call(PL310_ENABLE_WRITE, v); }


void Genode::pl310_aux(Pl310::Aux::access_t const v) {
	trustzone_hypervisor_call(PL310_AUX_WRITE, v); }


void Genode::pl310_debug(Pl310::Debug::access_t const v) {
	trustzone_hypervisor_call(PL310_DEBUG_WRITE


void Genode::cpu_actlr_smp_bit_raise() {
	trustzone_hypervisor_call(CPU_ACTLR_SMP_BIT_RAISE, 0); }
