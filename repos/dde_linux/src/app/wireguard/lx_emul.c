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


static struct rtnl_link_ops *_wireguard_rtnl_link_ops;


struct rtnl_link_ops *wireguard_rtnl_link_ops()
{
	return _wireguard_rtnl_link_ops;
}


void lx_user_init(void) {}
void lx_emul_associate_page_selftest(void) {}
void lx_emul_forget_pages(void const *virt, unsigned long size) {}


#include <linux/slab.h>

void * kmalloc_order(size_t size,gfp_t flags,unsigned int order)
{
	return kmalloc(size, flags);
}


int rtnl_link_register(struct rtnl_link_ops * ops)
{
	_wireguard_rtnl_link_ops = ops;
	return 0;
}
