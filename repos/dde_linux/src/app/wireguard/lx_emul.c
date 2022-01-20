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

extern void genode_wg_udp_tunnel_sock_cfg(struct udp_tunnel_sock_cfg * cfg);

void setup_udp_tunnel_sock(struct net * net,struct socket * sock,struct udp_tunnel_sock_cfg * cfg)
{
	genode_wg_udp_tunnel_sock_cfg(cfg);
}


#include <linux/ipv6.h>

bool ipv6_mod_enabled(void)
{
	return false;
}


#include <net/udp_tunnel.h>

void udp_tunnel_xmit_skb(struct rtable * rt,struct sock * sk,struct sk_buff * skb,__be32 src,__be32 dst,__u8 tos,__u8 ttl,__be16 df,__be16 src_port,__be16 dst_port,bool xnet,bool nocheck)
{
	pr_info("Send packet over UDP tunnel");
}


#include <net/sock.h>

DEFINE_STATIC_KEY_FALSE(memalloc_socks_key);
EXPORT_SYMBOL_GPL(memalloc_socks_key);


#include <linux/slab.h>

struct kmem_cache * kmem_cache_create_usercopy(const char * name,
                                               unsigned int size,
                                               unsigned int align,
                                               slab_flags_t flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (* ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor);
}


#include <net/ip_tunnels.h>

/* Returns either the correct skb->protocol value, or 0 if invalid. */
__be16 ip_tunnel_parse_protocol(const struct sk_buff *skb)
{
	//FIXME: we just assume IPv4
	return htons(ETH_P_IP);
}


#include <linux/random.h>

bool rng_is_initialized(void)
{
	return true;
}


#include <asm/pgtable.h>

unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)] = { 0 };
