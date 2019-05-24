/*
 * \brief   Startup code for kernel
 * \author  Adrian-Ken Rueegsegger
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \author  Alexander Boettcher
 * \date    2015-02-06
 */

/*
 * Copyright (C) 2011-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.set STACK_SIZE, 4096

/* offsets of member variables in a CPU context */
.set IP_OFFSET, 17 * 8
.set SP_OFFSET, 20 * 8

/* .set BASE, 0x200028 */
.set BASE, 0x201cd0
.set ISR, BASE
.set ISR_ENTRY_SIZE, 12

.set IDT_FLAGS_PRIVILEGED,   0x8e01
.set IDT_FLAGS_UNPRIVILEGED, 0xee01

.macro _isr_entry
	.align 4, 0x90
.endm

.macro _exception vector
	_isr_entry
	push $0
	push $\vector
	jmp _kernel_entry
.endm

.macro _exception_with_code vector
	_isr_entry
	nop
	nop
	push $\vector
	jmp _kernel_entry
.endm

.macro _idt_entry addr flags
	.word \addr & 0xffff
	.word 0x0008
	.word \flags
	.word (\addr >> 16) & 0xffff
	.long \addr >> 32
	.long 0
.endm

.macro _load_address label reg
	mov \label@GOTPCREL(%rip), %\reg
.endm

.section ".text.crt0"

	/* magic multi-boot 1 header */
	.long 0x1badb002
	.long 0x0
	.long 0xe4524ffe

	.long 0x0 /* align to 8 byte for mbi2 */
	__mbi2_start:
	/* magic multi-boot 2 header */
	.long   0xe85250d6
	.long   0x0
	.long   (__mbi2_end - __mbi2_start)
	.long  -(0xe85250d6 + (__mbi2_end - __mbi2_start))
	/* end tag - type, flags, size */
	.word   0x0
	.word   0x0
	.long   0x8
	__mbi2_end:

/****************************** IDT *******************************/

	/*****************************************
	 ** Interrupt Descriptor Table (IDT)    **
	 ** See Intel SDM Vol. 3A, section 6.10 **
	 *****************************************/

	.global __idt
	.align 8
	__idt:

	.set isr_addr, ISR
	.rept 256
	_idt_entry isr_addr IDT_FLAGS_PRIVILEGED
	.endr

	.global __idt_end
	.align 8
	__idt_end:


.macro SETUP_PAGING
	/* Enable PAE (prerequisite for IA-32e mode) */
	movl $0x20, %eax
	movl %eax, %cr4

	/* Enable IA-32e mode and execute-disable */
	movl $0xc0000080, %ecx
	rdmsr
	btsl $8, %eax
	btsl $11, %eax
	wrmsr

	/* Enable paging, write protection and caching */
	xorl %eax, %eax
	btsl  $0, %eax /* protected mode */
	btsl $16, %eax /* write protect */
	btsl $31, %eax /* paging */
	movl %eax, %cr0
.endm


	/*******************************************************************
	 ** Startup code for non-primary CPU (AP - application processor) **
	 *******************************************************************/

.code16
	.global _ap
	_ap:

	/* Load initial pagetables */
	mov $_kernel_pml4, %eax
	mov %eax, %cr3

	/* setup paging */
	SETUP_PAGING

	/* setup GDT */
	lgdtl %cs:__gdt - _ap
	ljmpl $8, $_start64

__gdt:
	.word  55
	.long  __gdt_start


	/**************************************************************
	 ** Startup code for primary CPU (bootstrap processor - BSP) **
	 **************************************************************/

.code32
	.global _start
	_start:

	/* preserve multiboot magic value register, used below later */
	movl %eax, %esi

	/**
	 * zero-fill BSS segment
	 */
	leal _bss_start, %edi
	leal _bss_end, %ecx
	sub %edi, %ecx
	shr $2, %ecx
	xor %eax, %eax
	rep stosl

	/* Load initial pagetables */
	leal _kernel_pml4, %eax
	mov %eax, %cr3

	/* setup paging */
	SETUP_PAGING

	/* setup GDT */
	lgdt __gdt_ptr

	/* Indirect long jump to 64-bit code */
	ljmp $8, $_start64_bsp


.code64
	.align 8
	.globl _start64_bsp
	_start64_bsp:

	/* save rax & rbx, used to lookup multiboot structures */
	movq __initial_ax@GOTPCREL(%rip),%rax
	movq %rsi, (%rax)

	movq __initial_bx@GOTPCREL(%rip),%rax
	movq %rbx, (%rax)

	_start64:

	/*
	 * Set up segment selectors fs and gs to zero
	 * ignore ss, cs and ds because they are ignored by hardware in long mode
	 */
	mov $0x0, %eax
	mov %eax, %fs
	mov %eax, %gs

	/* increment CPU counter atomically */
	movq __cpus_booted@GOTPCREL(%rip),%rax
	movq $1, %rcx
	lock xaddq %rcx, (%rax)

	/* if more CPUs started than supported, then stop them */
	cmp $NR_OF_CPUS, %rcx
	jge 1f

	/* calculate stack depending on CPU counter */
	movq $STACK_SIZE, %rax
	inc  %rcx
	mulq %rcx
	movq %rax, %rcx
	leaq __bootstrap_stack@GOTPCREL(%rip),%rax
	movq (%rax), %rsp
	addq %rcx, %rsp

	/*
	 * Enable paging and FPU:
	 * PE, MP, NE, WP, PG
	 */
	mov $0x80010023, %rax
	mov %rax, %cr0

	/*
	 * OSFXSR and OSXMMEXCPT for SSE FPU support
	 */
	mov %cr4, %rax
	bts $9,   %rax
	bts $10,  %rax
	mov %rax, %cr4

	fninit

/***************************** uart test begin ****************************/
/*
movw   $0x3fc,-0x6(%rbp)
movb   $0x80,-0x34(%rbp)
movzbl -0x34(%rbp),%eax
movzwl -0x6(%rbp),%edx
out    %al,(%dx)

_label_b:
mov    -0x4(%rbp),%eax
lea    -0x1(%rax),%edx
mov    %edx,-0x4(%rbp)
test   %eax,%eax
setne  %al
test   %al,%al
je     _label_a

jmp    _label_b
_label_a:
movw   $0x3f9,-0x20(%rbp)
movb   $0x1,-0x27(%rbp)

movzbl -0x27(%rbp),%eax
movzwl -0x20(%rbp),%edx
out    %al,(%dx)
movw   $0x3fa,-0x1e(%rbp)
movb   $0x0,-0x28(%rbp)
movzbl -0x28(%rbp),%eax
movzwl -0x1e(%rbp),%edx
out    %al,(%dx)
movw   $0x3fc,-0x1c(%rbp)
movb   $0x3,-0x29(%rbp)
movzbl -0x29(%rbp),%eax
movzwl -0x1c(%rbp),%edx
out    %al,(%dx)
movw   $0x3fa,-0x1a(%rbp)
movb   $0x0,-0x2a(%rbp)
movzbl -0x2a(%rbp),%eax
movzwl -0x1a(%rbp),%edx
out    %al,(%dx)
movw   $0x3fb,-0x18(%rbp)
movb   $0x7,-0x2b(%rbp)
movzbl -0x2b(%rbp),%eax
movzwl -0x18(%rbp),%edx
out    %al,(%dx)
movw   $0x3fd,-0x16(%rbp)
movb   $0xb,-0x2c(%rbp)
movzbl -0x2c(%rbp),%eax
movzwl -0x16(%rbp),%edx
out    %al,(%dx)
movw   $0x3fa,-0x14(%rbp)
movb   $0x1,-0x2d(%rbp)
movzbl -0x2d(%rbp),%eax
movzwl -0x14(%rbp),%edx
out    %al,(%dx)
movw   $0x3fa,-0x12(%rbp)

movzwl -0x12(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x2e(%rbp)
movw   $0x3fb,-0x10(%rbp)
movzwl -0x10(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x2f(%rbp)
movw   $0x3fc,-0xe(%rbp)
movzwl -0xe(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x30(%rbp)
movw   $0x3fd,-0xc(%rbp)
movzwl -0xc(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x31(%rbp)
movw   $0x3fe,-0xa(%rbp)
movzwl -0xa(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x32(%rbp)
movw   $0x3ff,-0x24(%rbp)
movzwl -0x24(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x33(%rbp)

movb   $0x20,-0x35(%rbp)
movw   $0x3fe,-0x22(%rbp)

_label_d:
movzwl -0x22(%rbp),%eax
mov    %eax,%edx
in     (%dx),%al
mov    %al,-0x26(%rbp)
movzbl -0x26(%rbp),%eax

and    -0x35(%rbp),%al
cmp    -0x35(%rbp),%al
setne  %al
test   %al,%al
je     _label_c
jmp    _label_d
_label_c:
movw   $0x3f9,-0x8(%rbp)
movb   $0x41,-0x25(%rbp)

movzbl -0x25(%rbp),%eax
movzwl -0x8(%rbp),%edx
out    %al,(%dx)
*/
/***************************** uart test end ****************************/












	/* kernel-initialization */
	call init

	/* catch erroneous return of the kernel initialization */
	1:
	hlt
	jmp 1b


	.global bootstrap_stack_size
	bootstrap_stack_size:
	.quad STACK_SIZE

	/******************************************
	 ** Global Descriptor Table (GDT)        **
	 ** See Intel SDM Vol. 3A, section 3.5.1 **
	 ******************************************/

	.align 4
	.space 2
	__gdt_ptr:
	.word 55 /* limit        */
	.long __gdt_start  /* base address */

	.set TSS_LIMIT, 0x68
	.set TSS_TYPE, 0x8900

	.align 8
	.global __gdt_start
	__gdt_start:
	/* gate 0: Null descriptor */
	.quad 0
	/* gate 1: 64-bit code segment descriptor */
	.long 0
	/* gate 1: GDTE_LONG | GDTE_PRESENT | GDTE_CODE | GDTE_NON_SYSTEM */
	.long 0x209800
	/* gate 2: 64-bit data segment descriptor */
	.long 0
	/* gate 2: GDTE_LONG | GDTE_PRESENT | GDTE_TYPE_DATA_A | GDTE_TYPE_DATA_W | GDTE_NON_SYSTEM */
	.long 0x209300
	/* gate 3: 64-bit user code segment descriptor */
	.long 0
	/* gate 3: GDTE_LONG | GDTE_PRESENT | GDTE_CODE | GDTE_NON_SYSTEM */
	.long 0x20f800
	/* gate 4: 64-bit user data segment descriptor */
	.long 0
	/* gate 4: GDTE_LONG | GDTE_PRESENT | GDTE_TYPE_DATA_A | GDTE_TYPE_DATA_W | GDTE_NON_SYSTEM */
	.long 0x20f300
	/* gate 5: Task segment descriptor */
	.long TSS_LIMIT
	/* gate 5: GDTE_PRESENT | GDTE_SYS_TSS */
	.long TSS_TYPE
	/* gate 6: Null descriptor */
	.long 0
	.long 0
	.global __gdt_end
	__gdt_end:


/*********************************
 ** .bss (non-initialized data) **
 *********************************/

.bss

	/* stack of the temporary initial environment */
	.p2align 12
	.globl __bootstrap_stack
	__bootstrap_stack:
	.rept NR_OF_CPUS
		.space STACK_SIZE
	.endr

	.globl __initial_ax
	__initial_ax:
	.space 8
	.globl __initial_bx
	__initial_bx:
	.space 8
	.globl __cpus_booted
	__cpus_booted:
	.quad 0
