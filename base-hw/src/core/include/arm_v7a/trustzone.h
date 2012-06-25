/*
 * \brief  Armv7-specific Trustzone functions
 * \author Stefan Kalkowski
 * \date   2012-06-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BASE_HW__SRC__CORE__INCLUDE__ARM_V7A__TRUSTZONE_H_
#define _BASE_HW__SRC__CORE__INCLUDE__ARM_V7A__TRUSTZONE_H_

#include <base/vm_state.h>

/**
 * Inspired by Fiasco.OC kernel src/kern/arm/cpu_arm.cpp thanks to Torsten Frenzel!
 */
static inline void tz_switch_to_normal_world(Genode::Vm_state *state)
{
	extern char _mm_switch_to_normal;

	asm volatile("stmdb sp!, {r0-r12}    \n" /* save registers           */
				 "stmdb sp!, {%[state]}  \n" /* save vm_state pointer    */
				 "mov    r2, sp          \n" /* copy sp_svc to sp_mon    */
				 "cps    #22             \n" /* switch to monitor mode   */
				 "mov    sp, r2          \n"
				 "adr    r3, 1f          \n" /* save return eip          */
				 "mrs    r4, cpsr        \n" /* save return psr          */
				 "mov    pc, %[instr]    \n" /* world switch routine     */
				 "1:                     \n"
				 "mov    r0, sp          \n" /* copy sp_mon to sp_svc    */
				 "cps    #19             \n" /* switch to svc mode       */
				 "mov    sp, r0          \n"
				 "ldmia  sp!, {%[state]} \n" /* restore vm_state pointer */
				 "ldmia  sp!, {r0-r12}   \n" /* restore registers        */
				 : : [state] "r" (state), [instr] "r" (&_mm_switch_to_normal)
				 : "memory");
}

#endif /* _BASE_HW__SRC__CORE__INCLUDE__ARM_V7A__TRUSTZONE_H_ */
