#ifndef _LX_EMUL__SHADOW__ASM__PGTABLE_H_
#define _LX_EMUL__SHADOW__ASM__PGTABLE_H_

#include_next <asm/pgtable.h>

static inline struct page * __dummy_zero_page(void)
{
	//static struct page zero;
	//zero.virtual = empty_zero_page;
	return (struct page *)empty_zero_page;
}

#undef  ZERO_PAGE
#define ZERO_PAGE(vaddr) __dummy_zero_page()

#endif /* _LX_EMUL__SHADOW__ASM__PGTABLE_H_ */
