/**
 * \brief  Dummy definitions of lx_emul
 * \author Stefan Kalkowski
 * \date   2022-01-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* app/wireguard includes */
#include <lx_emul.h>


void lx_user_init(void) {}
void lx_emul_associate_page_selftest(void) {}
void lx_emul_forget_pages(void const *virt, unsigned long size) {}


#include <lx_emul/random.h>
#include <linux/random.h>

void get_random_bytes(void * buf,int nbytes)
{
	lx_emul_random_bytes(buf, nbytes);
}


#include <linux/slab.h>

void * kmalloc_order(size_t size,gfp_t flags,unsigned int order)
{
	return kmalloc(size, flags);
}


#include <linux/mm.h>

void * kvmalloc_node(size_t size,gfp_t flags,int node)
{
	return kmalloc(size, flags);
}


#include <net/rtnetlink.h>

extern void genode_wg_rtnl_link_ops(struct rtnl_link_ops * ops);

int rtnl_link_register(struct rtnl_link_ops * ops)
{
	genode_wg_rtnl_link_ops(ops);
	return 0;
}


#include <net/genetlink.h>

extern void genode_wg_genl_family(struct genl_family * family);

int genl_register_family(struct genl_family * family)
{
	genode_wg_genl_family(family);
	return 0;
}
