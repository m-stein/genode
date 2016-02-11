.section ".text"

.global _start
_start:

	/* load stack pointer */
	lla sp, _stack_high

	/* relocate linker */
	jal init_rtld

	/* create environment for main thread */
	jal init_main_thread

	/* load stack pointer from init_main_thread_result */
	la sp, init_main_thread_result
	ld sp, (sp)

	mv s0, x0

	/* jump into init C-code */
	j _main

.bss
	.p2align 8
	.space 32*1024
	_stack_high:
