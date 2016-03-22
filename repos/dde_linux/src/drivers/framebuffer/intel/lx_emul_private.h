/*
 * \brief  Local definitions of the Linux kernel API implementation
 * \author Norman Feske
 * \date   2015-08-24
 */

#ifndef _LX_EMUL_PRIVATE_H_
#define _LX_EMUL_PRIVATE_H_

/* Linux kernel API */
#include <stdarg.h>
#include <lx_emul/printf.h>

#if __x86_64__
	#define ASM_FP(fp) asm volatile ("movq %%rbp, %0" : "=r"(fp) : :);
#else
	#define ASM_FP(fp) asm volatile ("movl %%ebp, %0" : "=r"(fp) : :);
#endif

#define PRINT_BACKTRACE \
do { \
	unsigned long * fp; \
	lx_printf("backtrace follows:\n"); \
	ASM_FP(fp); \
	while (fp && *(fp + 1)) { \
		lx_printf("%lx\n", *(fp + 1)); \
		fp = (unsigned long*)*fp; \
	} \
} while(0)

#if 1
#define TRACE \
	do { \
		lx_printf("%s not implemented\n", __func__); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
	do { \
		lx_printf("%s not implemented\n", __func__); \
		PRINT_BACKTRACE; \
		BUG(); \
	} while (0)

#define ASSERT(x) \
	do { \
		if (!(x)) { \
			lx_printf("%s:%u assertion failed\n", __func__, __LINE__); \
			BUG(); \
		} \
	} while (0)

#endif /* _LX_EMUL_PRIVATE_H_ */
