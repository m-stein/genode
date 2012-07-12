/*
 * \brief  Monitor mode vector table
 * \author Stefan Kalkowski
 * \date   2012-06-14
 *
 * A lot of the assembler routines found here ground on the work of
 * Torsten Frenzel that can be found in the Fiasco.OC kernel
 * (src/kern/arm/ivt.S) which is licensed under GPL.
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


.macro _mm_restore_bank mode
	cps   #\mode                    /* switch to given mode                  */
	ldmia r0!, {sp, lr}             /* load mode-specific sp and lr          */
	ldmia r0!, {r1}                 /* load mode-specific spsr               */
	msr   spsr, r1
.endm


.macro _mm_store_bank mode
	cps   #19                       /* switch to given mode                  */
	stmia r0!, {sp, lr}             /* store mode-specific sp and lr         */
	mrs   r1, spsr                  /* store mode-specific spsr              */
	stmia r0!, {r1}
.endm


.macro _mm_toggle_ns_bit reg bit
	mov   \reg, #\bit
	mcr   p15, 0, \reg, c1, c1, 0
	isb
.endm


/**
 * Save secure state on top of the stack.
 *
 * We save also the user-level registers here, because we need to
 * restore some on FIQ.
 */
.macro _mm_save_secure_state
	stmdb sp!, {r3, r4}             /* save cpsr and return-eip of svc       */
	stmdb sp,  {sp, lr}^            /* save user-level return values         */
	sub   sp,  sp, #8               /* adjust stack pointer                  */
.endm


.macro _mm_restore_normal_state
	ldr   r0, [sp, #16]             /* load vm_state addr. from stack        */
	add   r0, r0, #13*4             /* jump over general purpose register    */
	ldmia r0, {sp, lr}^             /* sp_usr and lr_usr                     */
	add   r0, r0, #8                /* adjust offset in r0                   */
	_mm_restore_bank 18             /* load irq banks                        */
	cps   #17                       /* switch to fiq mode                    */
	ldmia r0!, {r8 - r12, sp, lr}   /* load fiq registers                    */
	ldmia r0!, {r1}                 /* load spsr_fiq                         */
	msr   spsr, r1
	_mm_restore_bank 23             /* load abort banks                      */
	_mm_restore_bank 27             /* load undefined banks                  */
	_mm_restore_bank 19             /* load supervisor banks                 */
	cps   #22                       /* switch back to monitor mode           */
	ldmia r0!, {r1, r2}             /* copy return pc/cpsr on stack          */
	stmdb sp,  {r1, r2}
	_mm_toggle_ns_bit r1, 1
	ldmia r0!, {r1}                 /* load floating-point excp. register    */
#	mcr   p10, 7, r1, cr8, cr0, 0
	_mm_toggle_ns_bit r1, 0
	ldr   lr, [sp, #16]             /* load general-purpose register         */
	ldmia lr!, {r0 - r12}
.endm


.macro _mm_save_normal_state
	str   lr, [sp,#-12]             /* save exit reason temporarily on stack */
	_mm_toggle_ns_bit lr, 0
	ldr   lr, [sp, #16]             /* save general-purpose registers        */
	stmia lr!, {r0 - r12}
	mov   r0, lr
	stmia r0, {sp, lr}^             /* save sp_usr and lr_usr                */
	add   r0, r0, #8                /* adjust offset                         */
	_mm_store_bank 18               /* save irq banks                        */
	cps   #17                       /* switch to fiq mode                    */
	stmia r0!, {r8 - r12, sp, lr}
	mrs   r1, spsr
	stmia r0!, {r1}
	_mm_store_bank 23               /* save abort banks                      */
	_mm_store_bank 27               /* save undefined banks                  */
	_mm_store_bank 19               /* save supervisor banks                 */
	cps   #22                       /* switch to monitor mode                */
	sub   lr, sp, #8                /* copy return pc/cpsr from stack        */
	ldmia lr, {r1, r2}
	stmia r0!, {r1, r2}
	_mm_toggle_ns_bit r1, 1
#	mrc   p10, 7, r1, cr8, cr0, 0   /* store floating-point exc. register    */
	stmia r0!, {r1}
	_mm_toggle_ns_bit r1, 0
	ldr   r1, [sp, #-12]            /* copy the exit reason from stack       */
	stmia r0!,{r1}
.endm


.macro _mm_restore_secure_state
	mov   r0, sp                    /* copy sp to supervisor mode            */
	cps   #19                       /* switch to supervisor mode             */
	mov   sp, r0
	cps   #22                       /* switch back to monitor mode           */
	ldmia sp, {sp, lr}^             /* restore user-level return values      */
	add   sp, sp, #8
	ldmia sp!, {r3, r4}             /* restore cpsr and eip from supervisor  */
.endm


.macro _mm_switch_to_secure nr, off
	sub   lr, lr, #\off             /* adjust return eip according to exc.   */
	srsdb sp, #22                   /* store return state on mon_sp          */
	mov   lr, #\nr                  /* store exc. number in lr               */
	_mm_save_normal_state
	_mm_restore_secure_state
	mov   lr, r3
	msr   spsr_cfsx, r4             /* write cfsx flags to spsr register     */
	movs  pc, lr
.endm


/************************************************
 **  Monitor mode exception vector definition  **
 ************************************************/

.p2align 5
.globl _mm_vector_base
_mm_vector_base:
		nop                         /* not used                              */
		nop                         /* not used                              */
		b   _mm_smc_entry           /* secure monitor call                   */
		b   _mm_pf_abort_entry      /* prefetch abort                        */
		b   _mm_data_abort_entry    /* data abort                            */
		nop                         /* not used                              */
		b   _mm_irq_entry           /* interrupt                             */
		b   _mm_fiq_entry           /* fast interrupt                        */

.globl _mm_switch_to_normal
_mm_switch_to_normal:
	_mm_save_secure_state
	_mm_restore_normal_state
	mov lr, #127
#	mov lr, #125
	mcr   p15, 0, lr, c1, c1, 0 /* enable AW, FW, EA, FIQ, and NS bit        */
#	_mm_toggle_ns_bit lr, 1     /* switch to non-secure mode                 */
	ldr   lr, [sp, #-4]
	msr   spsr, lr              /* set spsr_mon with unsecure spsr           */
	ldr   lr, [sp, #-8]         /* set lr_mon with unsecure ip               */
	subs  pc, lr, #0

_mm_smc_entry:        _mm_switch_to_secure 2 0
_mm_pf_abort_entry:   _mm_switch_to_secure 3 4
_mm_data_abort_entry: _mm_switch_to_secure 4 4
_mm_irq_entry:        _mm_switch_to_secure 6 4
_mm_fiq_entry:        _mm_switch_to_secure 7 4
