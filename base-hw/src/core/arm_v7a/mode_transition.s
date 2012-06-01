/*
 * \brief  Transition between kernel and userland
 * \author Martin stein
 * \date   2011-11-15
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/**
 * Invalidate all entries of the branch predictor array
 */
.macro _flush_branch_predictor
	mcr p15, 0, sp, c7, c5, 6
	isb
.endm

/**
 * Switch from an interrupted user context to a kernel context
 *
 * \param  exception_type  Immediate exception type ID
 * \param  pc_adjust       Immediate value that gets subtracted from the
 *                         user PC before it gets saved
 */
.macro _user_to_kernel_pic exception_type, pc_adjust

	/**************************************************
	 ** We're already in the user protection domain, **
	 ** so we must avoid access to kernel memory     **
	 **************************************************/

	/* Load kernel contextidr */
	adr sp, _mt_kernel_context_begin
	ldr sp, [sp, #17*4]
	mcr p15, 0, sp, c13, c0, 1
	_flush_branch_predictor

	/* Load kernel section table */
	adr sp, _mt_kernel_context_begin
	ldr sp, [sp, #19*4]
	mcr p15, 0, sp, c2, c0, 0
	_flush_branch_predictor

	/*******************************************
	 ** Now it's save to access kernel memory **
	 *******************************************/

	/* Get user context pointer */
	ldr sp, _mt_user_context_ptr

	/* Save user r0 ... r12 */
	stmia sp, {r0-r12}

	/* Save user lr and sp */
	add r0, sp, #13*4
	stmia r0, {sp,lr}^

	/* Adjust and save user pc */
	.if \pc_adjust != 0
		sub lr, lr, #\pc_adjust
	.endif
	str lr, [sp, #15*4]

	/* Save user psr */
	mrs r0, spsr
	str r0, [sp, #16*4]

	/* Save type of exception that interrupted the user */
	mov r0, #\exception_type
	str r0, [sp, #18*4]

	/* Get kernel context pointer */
	adr r0, _mt_kernel_context_begin

	/* Load kernel context */
	add r0, r0, #13*4
	ldmia r0, {sp, lr, pc}

.endm


.section .text

	/* Mode transition PIC switches between a kernel context and a user context
	 * and thereby between their address spaces, thus it must be mapped
	 * executable to the same region in every address space */
	.align 3
	.global _mode_transition_begin
	_mode_transition_begin:

		/* On user exceptions the CPU has to jump to one of the following
		 * 7 entry vectors to switch to a kernel context */
		.align 3
		.global _mt_kernel_entry_pic
		_mt_kernel_entry_pic:

			b _rst_entry  /* Reset                  */
			b _und_entry  /* Undefined instruction  */
			b _svc_entry  /* Supervisor call        */
			b _pab_entry  /* Prefetch abort         */
			b _dab_entry  /* Data abort             */
			nop           /* Reserved               */
			b _irq_entry  /* Interrupt request      */
			b _fiq_entry  /* Fast interrupt request */

			/* PICs that switch from an user exception to the kernel */
			_rst_entry: _user_to_kernel_pic 1, 0
			_und_entry: _user_to_kernel_pic 2, 4
			_svc_entry: _user_to_kernel_pic 3, 0
			_pab_entry: _user_to_kernel_pic 4, 4
			_dab_entry: _user_to_kernel_pic 5, 8
			_irq_entry: _user_to_kernel_pic 6, 4
			_fiq_entry: _user_to_kernel_pic 7, 4

		/* Kernel must jump to this point to switch to a user context */
		.align 3
		.global _mt_user_entry_pic
		_mt_user_entry_pic:

			/* Get user context pointer */
			ldr lr, _mt_user_context_ptr

			/* Buffer user pc */
			ldr r0, [lr, #15*4]
			adr r1, _mt_buffer
			str r0, [r1]

			/* Buffer user psr */
			ldr r0, [lr, #16*4]
			msr spsr, r0

			/* Load user r0 ... r12 */
			ldmia lr, {r0-r12}

			/* Load user sp and lr */
			add sp, lr, #13*4
			ldmia sp, {sp,lr}^

			/* Get user contextidr and section table */
			ldr sp, [lr, #17*4]
			ldr lr, [lr, #19*4]

			/********************************************************
			 ** From now on, until we leave kernel mode, we must   **
			 ** avoid access to memory that is not mapped globally **
			 ********************************************************/

			/* Apply user contextidr and section table */
			mcr p15, 0, sp, c13, c0, 1
			mcr p15, 0, lr, c2, c0, 0
			_flush_branch_predictor

			/* Load user pc (implies application of the user psr) */
			adr lr, _mt_buffer
			ldmia lr, {pc}^

		/* Leave some space for the kernel context */
		.align 3
		.global _mt_kernel_context_begin
		_mt_kernel_context_begin: .space 32*4
		.global _mt_kernel_context_end
		_mt_kernel_context_end:

		/* Pointer to the user context backup space */
		.align 3
		.global _mt_user_context_ptr
		_mt_user_context_ptr: .long 0

		/* A local word-sized buffer */
		.align 3
		.global _mt_buffer
		_mt_buffer: .long 0

	.align 3
	.global _mode_transition_end
	_mode_transition_end:

