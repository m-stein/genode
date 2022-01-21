#ifndef _LX_EMUL__SHADOW__LINUX_MM_H_
#define _LX_EMUL__SHADOW__LINUX_MM_H_

#include_next <linux/mm.h>

static inline void * __dummy_page_address(const struct page * page)
{
	return (void *)page;
}

#undef  page_address
#define page_address(page) __dummy_page_address(page)

#endif /* _LX_EMUL__SHADOW__LINUX_MM_H_ */

