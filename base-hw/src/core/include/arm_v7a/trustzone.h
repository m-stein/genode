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


/**
 * Taken from Fiasco.OC kernel src/kern/arm/cpu_arm.cpp thanks to Torsten Frenzel!
 */
static inline void tz_switch_to_normal_world(Vm_state *state)
{
	volatile register addr_t r0 asm("r0") = (addr_t)state;
	extern char _mm_switch_to_normal;

	asm volatile("stmdb sp!, {fp}   \n" /* save frame-pointer       */
				 "stmdb sp!, {r0}   \n" /* save vm_state pointer    */
				 "mov    r2, sp     \n" /* copy sp_svc to sp_mon    */
				 "cps    #22        \n" /* switch to monitor mode   */
				 "mov    sp, r2     \n"
				 "adr    r3, 1f     \n" /* save return eip          */
				 "mrs    r4, cpsr   \n" /* save return psr          */
				 "mov    pc, r1     \n" /* world switch routine     */
				 "1:                \n"
				 "mov    r0, sp     \n" /* copy sp_mon to sp_svc    */
				 "cps    #19        \n" /* switch to svc mode       */
				 "mov    sp, r0     \n"
				 "ldmia  sp!, {r0}  \n" /* restore vm_state pointer */
				 "ldmia  sp!, {fp}  \n" /* restore frame-pointer    */
				 : : "r" (r0), "r" (&_mm_switch_to_normal)
				 : "r2", "r3", "r4", "r5", "r6", "r7",
				   "r8", "r9", "r10", "r12", "r14", "memory");
}

#endif /* _BASE_HW__SRC__CORE__INCLUDE__ARM_V7A__TRUSTZONE_H_ */
