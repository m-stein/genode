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
#include <board.h>

using namespace Genode;

enum Trustzone_hypervisor_syscalls
{
	L2_CACHE_SET_DEBUG_REG = 0x100,
	L2_CACHE_ENABLE_REG    = 0x102,
	L2_CACHE_AUX_REG       = 0x109,
};

static inline void trustzone_hypervisor_call(addr_t func, addr_t val)
{
	register addr_t _func asm("r12") = func;
	register addr_t _val  asm("r0")  = val;
	asm volatile("dsb; smc #0" :: "r" (_func), "r" (_val)
	             : "memory", "cc", "r1", "r2", "r3", "r4", "r5",
	               "r6", "r7", "r8", "r9", "r10", "r11");
}


void Genode::l2_cache_enable(bool const enable) {
	trustzone_hypervisor_call(L2_CACHE_ENABLE_REG, enable); }


void Genode::l2_cache_aux_reg(addr_t const v) {
	trustzone_hypervisor_call(L2_CACHE_AUX_REG, v); }


void Genode::l2_cache_debug_reg(addr_t const v) {
	trustzone_hypervisor_call(L2_CACHE_SET_DEBUG_REG, v); }
