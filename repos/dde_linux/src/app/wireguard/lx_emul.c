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


#include <linux/random.h>

int wait_for_random_bytes(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/random.h>

u32 get_random_u32(void)
{
	u8 buf[4];
	lx_emul_random_bytes(buf, sizeof(buf));
	return *((u32*)&buf);
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
#include <genode_c_api/wireguard.h>

void udp_tunnel_xmit_skb(
	struct rtable * rt,struct sock * sk,struct sk_buff * skb,
	__be32 src,__be32 dst,__u8 tos,__u8 ttl,__be16 df,
	__be16 src_port,__be16 dst_port,bool xnet,bool nocheck)
{
	/*
	 * FIXME
	 *
	 * I don't propagate these values because I found that, at the call to
	 * udp_tunnel_xmit_skb, Linux sets them hard to the values I'm checking
	 * for in the following. Furthermore, I assume that they should not be
	 * relevant for the port anyway.
	 */
	if (xnet != false) {
		pr_info("Error: XNET != false is not expected\n");
		while (1) { }
	}
	if (nocheck != false) {
		pr_info("Error: NOCHECK != false is not expected\n");
		while (1) { }
	}
	if (df != 0) {
		pr_info("Error: DF != 0 is not expected\n");
		while (1) { }
	}
	/*
	 * FIXME
	 *
	 * Manually set TTL. In the Linux reference scenario this argument is
	 * observed to be 64. Here it is 0. Further Linux contrib code would have
	 * to be incorporated in order to make Wireguard provide a correct TTL
	 * argument. However, it is simpler to set it manually.
	 */
	if (ttl != 0) {
		pr_info("Error: TTL != 0 is not expected\n");
		while (1) { }
	}
	ttl = 64;

	/*
	 * FIXME
	 *
	 * Manually set UDP destination port. In the Linux reference scenario this
	 * argument is observed to be the listen port. Here it is som other port.
	 * Further Linux contrib code would have to be incorporated in order to
	 * make Wireguard provide a correct argument. However, it is simpler to
	 * set it manually.
	 */
	src_port = htons(genode_wg_listen_port());

	genode_wg_send_wg_prot_at_nic_connection(
		skb->data, skb->len, src_port, dst_port, src, dst,
		tos, ttl);

/* handshake response

	total pkt size == 134 bytes

	src      == 0x302000a big endian == 10.0.2.3
	dst      == 0x102000a big endian == 10.0.2.1
	tos      == 0x88, type of service field of ipv4 header
	ttl      == 0x40, time to live field of ipv4 header
	df       == 0, dont fragment field of ipv4 header
	src_port == 0x6abf big endian == 49002, udp source port
	dst_port == 0x69bf big endian == 49001, udp dest port
	xnet     == 0, ?
	nocheck  == 0, ?

	skb->data[0]..skb->data[skb->len-1]:
	  0:  2  0  0  0 <--- wireguard prot starts at idx 0
	  4:  5 ae 65 ca
	  8: 81 42 e9 48
	 12: 1b bd  c 3a
	 16: ed 3f b9 4c
	 20: 70 91 f6 e3
	 24: 8d 3f 77 1c
	 28: ed e5 35 51
	 32: b4 12 21 91
	 36: df fb c8 1c
	 40: 57 22  a  5
	 44: de 50 8b c7
	 48: 23 77 a0 25
	 52: aa 53 5f 99
	 56: 12 29 54 15
	 60: ab c9 97 f3
	 64: dd 20 a5 ec
	 68: 5f c1 8e 74
	 72: 9d 41 b8 9c
	 76:  0  0  0  0
	 80:  0  0  0  0
	 84:  0  0  0  0
	 88:  0  0  0  0 <--- wireguard prot ends at idx 91

	The wireguard prot is not tunneled or encrypted but only encapsuled in
	normal udp.

	A wireshark hex dump of the whole handshake response

0000   72 ae 11 31 58 a7 52 54 00 12 34 56 08 00 45 88
0010   00 78 a0 9e 00 00 40 11 c1 4b 0a 00 02 03 0a 00
             -----             ----- -----------
             ip.id             ip.crc     ip.src
0020   02 01 bf 6a bf 69 00 64 f9 e1 02 00 00 00 14 22
             -----
             udp.src
0030   a9 bc aa 7b c2 f6 fe c4 ea 78 a1 1d 5f 4d 73 0f
0040   cd d8 8e a7 40 7c 80 2a b7 d4 9e c0 a0 a3 6c 7f
0050   ac a7 49 cc 59 50 70 9c 09 06 82 32 d6 7f 42 3e
0060   42 24 1a c1 6c 02 02 0b c5 c2 c9 e1 dd fc 5a 4d
0070   43 30 61 88 c7 91 00 00 00 00 00 00 00 00 00 00
0080   00 00 00 00 00 00

------------------------------------------------------

0000   82 f1 5b 0b 24 04 52 54 00 12 34 56 08 00 45 88
0010   00 78 00 00 00 00 40 11 60 eb 0a 00 03 02 0a 00
             -----             ----- -----------
             ip.id             ip.crc     ip.src
0020   02 01 b0 48 bf 69 00 64 56 1d 02 00 00 00 25 ba
             -----             ----- |
             udp.src         udp.crc wg start
0030   46 38 c2 47 2b dd 0d d0 f2 19 4d 07 72 55 82 df
0040   dc 32 42 51 74 c7 b9 15 ef fb 8f c1 96 eb a6 4c
0050   89 8a 7c d5 c5 77 76 aa dc e8 e7 be f6 18 aa 55
0060   2d 2d ce ed ed 40 bd 34 ac e7 64 2b b5 1f 71 a2
0070   3b b4 64 5b 52 b1 00 00 00 00 00 00 00 00 00 00
0080   00 00 00 00 00 00
                       |
                       wg end



*/

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


#include <linux/inetdevice.h>

__be32 inet_confirm_addr(struct net * net,struct in_device * in_dev,__be32 dst,__be32 local,int scope)
{
	lx_emul_trace(__func__);
	return local;
}


#include <net/route.h>

struct rtable * ip_route_output_flow(struct net * net,struct flowi4 * flp4,const struct sock * sk)
{
	static bool initialized = false;
	static struct dst_metrics dst_default_metrics;
	static struct rtable rt;
	if (!initialized) {
		rt.dst.dev = genode_wg_net_device();
		dst_init_metrics(&rt.dst, dst_default_metrics.metrics, true);
		initialized = true;
	}
	return &rt;
}


#include <linux/sched.h>

int __cond_resched(void)
{
	if (should_resched(0)) {
		schedule();
		return 1;
	}
	return 0;
}


#include <linux/rcupdate.h>

void call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	func(head);
}


#include <linux/slab.h>

void kfree_sensitive(const void * p)
{
	kfree(p);
}

