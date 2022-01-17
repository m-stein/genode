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


#include <linux/netdevice.h>

extern struct net_device * genode_wg_net_device(void);

struct net_device * dev_get_by_name(struct net * net,const char * name)
{
	return genode_wg_net_device();
}


#include <net/udp_tunnel.h>

int udp_sock_create4(struct net * net,struct udp_port_cfg * cfg,struct socket ** sockp)
{
	*sockp = (struct socket*) kmalloc(sizeof(struct socket), GFP_KERNEL);
	(*sockp)->sk = (struct sock*) kmalloc(sizeof(struct sock), GFP_KERNEL);
	return 0;
}


#include <net/udp_tunnel.h>

void setup_udp_tunnel_sock(struct net * net,struct socket * sock,struct udp_tunnel_sock_cfg * cfg)
{
	lx_emul_trace(__func__);
}


#include <linux/ipv6.h>

bool ipv6_mod_enabled(void)
{
	return false;
}

