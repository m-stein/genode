/*
 * \brief  Kernel environment
 * \author Martin Stein
 * \date   2019-06-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <stdio.h>
#include <types.h>

#ifndef _KENV_H_
#define _KENV_H_

enum { KENV_MVALLEN = 128 };

enum {
	KENV_GET = 0
};

struct dmi_header;

void genode_free(void * buf) { };

void *genode_mem_chunk(size_t base, size_t len, const char *devmem);

void genode_dmi_table(unsigned long base, uint32_t version);

void genode_dmi_header(struct dmi_header const *header);

int kenv(int action, const char *name, char *value, int len)
{
	fprintf(stderr, "%s called (from %p): action %d name %s, not implemented\n", __func__, __builtin_return_address(0), action, name);
	return -1;
}

#endif /* _KENV_H_ */
