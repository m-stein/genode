/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2022-01-11
 */

#include <lx_emul.h>


#include <linux/skbuff.h>

int ___pskb_trim(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/percpu.h>

void __percpu * __alloc_percpu(size_t size,size_t align)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/percpu.h>

void __percpu * __alloc_percpu_gfp(size_t size,size_t align,gfp_t gfp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

struct sk_buff * __alloc_skb(unsigned int size,gfp_t gfp_mask,int flags,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

int __sched __cond_resched(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

struct cpumask __cpu_online_mask;


#include <linux/cpumask.h>

struct cpumask __cpu_possible_mask;


#include <crypto/algapi.h>

noinline unsigned long __crypto_memneq(const void * a,const void * b,size_t size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/icmp.h>

void __icmp_send(struct sk_buff * skb_in,int type,int code,__be32 info,const struct ip_options * opt)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwsem.h>

void __init_rwsem(struct rw_semaphore * sem,const char * name,struct lock_class_key * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6.h>

int __ipv6_addr_type(const struct in6_addr * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

struct irq_domain * __irq_domain_add(struct fwnode_handle * fwnode,int size,irq_hw_number_t hwirq_max,int direct_max,const struct irq_domain_ops * ops,void * host_data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

struct irq_desc * __irq_resolve_mapping(struct irq_domain * domain,irq_hw_number_t hwirq,unsigned int * irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/bottom_half.h>

void __local_bh_enable_ip(unsigned long ip,unsigned int cnt)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mutex.h>

void __mutex_init(struct mutex * lock,const char * name,struct lock_class_key * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void __napi_schedule(struct napi_struct * n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void __netif_napi_del(struct napi_struct * napi)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int __nla_parse(struct nlattr ** tb,int maxtype,const struct nlattr * head,int len,const struct nla_policy * policy,unsigned int validate,struct netlink_ext_ack * extack)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * __pskb_pull_tail(struct sk_buff * skb,int delta)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

bool __skb_flow_dissect(const struct net * net,const struct sk_buff * skb,struct flow_dissector * flow_dissector,void * target_container,const void * data,__be16 proto,int nhoff,int hlen,unsigned int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void __skb_get_hash(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct sk_buff * __skb_gso_segment(struct sk_buff * skb,netdev_features_t features,bool tx_path)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

atomic_long_t _totalram_pages;


extern void ack_bad_irq(unsigned int irq);
void ack_bad_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

__printf (1,4)struct workqueue_struct * alloc_workqueue(const char * fmt,unsigned int flags,int max_active,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcupdate.h>

void call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool cancel_delayed_work_sync(struct delayed_work * dwork)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/chacha20poly1305.h>

bool chacha20poly1305_decrypt(u8 * dst,const u8 * src,const size_t src_len,const u8 * ad,const size_t ad_len,const u64 nonce,const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/chacha20poly1305.h>

bool chacha20poly1305_decrypt_sg_inplace(struct scatterlist * src,size_t src_len,const u8 * ad,const size_t ad_len,const u64 nonce,const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/chacha20poly1305.h>

void chacha20poly1305_encrypt(u8 * dst,const u8 * src,const size_t src_len,const u8 * ad,const size_t ad_len,const u64 nonce,const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/chacha20poly1305.h>

bool chacha20poly1305_encrypt_sg_inplace(struct scatterlist * src,size_t src_len,const u8 * ad,const size_t ad_len,const u64 nonce,const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


extern int cpu_has_xfeatures(u64 xfeatures_needed,const char ** feature_name);
int cpu_has_xfeatures(u64 xfeatures_needed,const char ** feature_name)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/curve25519.h>

void curve25519_arch(u8 mypublic[CURVE25519_KEY_SIZE],const u8 secret[CURVE25519_KEY_SIZE],const u8 basepoint[CURVE25519_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/curve25519.h>

void curve25519_base_arch(u8 pub[CURVE25519_KEY_SIZE],const u8 secret[CURVE25519_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/curve25519.h>

const u8 curve25519_null_point[] = {};


#include <linux/timer.h>

int del_timer(struct timer_list * timer)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

void delayed_work_timer_fn(struct timer_list * t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

void destroy_workqueue(struct workqueue_struct * wq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct net_device * dev_get_by_index(struct net * net,int ifindex)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

struct net_device * dev_get_by_name(struct net * net,const char * name)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void dev_get_tstats64(struct net_device * dev,struct rtnl_link_stats64 * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

void __noreturn do_exit(long code)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwsem.h>

void __sched down_read(struct rw_semaphore * sem)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwsem.h>

void __sched down_write(struct rw_semaphore * sem)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst_cache.h>

void dst_cache_destroy(struct dst_cache * dst_cache)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst_cache.h>

struct rtable * dst_cache_get_ip4(struct dst_cache * dst_cache,__be32 * saddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst_cache.h>

struct dst_entry * dst_cache_get_ip6(struct dst_cache * dst_cache,struct in6_addr * saddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst_cache.h>

int dst_cache_init(struct dst_cache * dst_cache,gfp_t gfp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst_cache.h>

void dst_cache_set_ip4(struct dst_cache * dst_cache,struct dst_entry * dst,__be32 saddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst_cache.h>

void dst_cache_set_ip6(struct dst_cache * dst_cache,struct dst_entry * dst,const struct in6_addr * saddr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/dst.h>

void dst_release(struct dst_entry * dst)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/flow_dissector.h>

struct flow_dissector flow_keys_basic_dissector;


#include <linux/workqueue.h>

bool flush_work(struct work_struct * work)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

void flush_workqueue(struct workqueue_struct * wq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void free_netdev(struct net_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/percpu.h>

void free_percpu(void __percpu * ptr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/genetlink.h>

void * genlmsg_put(struct sk_buff * skb,u32 portid,u32 seq,const struct genl_family * family,int flags,u8 cmd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/random.h>

void get_random_bytes(void * buf,int nbytes)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/random.h>

u32 get_random_u32(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/siphash.h>

u32 hsiphash_2u32(const u32 first,const u32 second,const hsiphash_key_t * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/siphash.h>

u32 hsiphash_3u32(const u32 first,const u32 second,const u32 third,const hsiphash_key_t * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/icmpv6.h>

void icmp6_send(struct sk_buff * skb,u8 type,u8 code,__u32 info,const struct in6_addr * force_saddr,const struct inet6_skb_parm * parm)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/in6.h>

const struct in6_addr in6addr_any;


#include <linux/inetdevice.h>

__be32 inet_confirm_addr(struct net * net,struct in_device * in_dev,__be32 dst,__be32 local,int scope)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netfilter/nf_conntrack.h>

struct net init_net;


#include <linux/timer.h>

void init_timer_key(struct timer_list * timer,void (* func)(struct timer_list *),unsigned int flags,const char * name,struct lock_class_key * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

long __sched io_schedule_timeout(long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ipv6.h>

int ip6_dst_hoplimit(struct dst_entry * dst)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/route.h>

struct rtable * ip_route_output_flow(struct net * net,struct flowi4 * flp4,const struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/ip_tunnels.h>

const struct header_ops ip_tunnel_header_ops;


#include <net/ip_tunnels.h>

__be16 ip_tunnel_parse_protocol(const struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/addrconf.h>

int ipv6_chk_addr(struct net * net,const struct in6_addr * addr,const struct net_device * dev,int strict)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/ipv6.h>

bool ipv6_mod_enabled(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

void irq_domain_free_irqs_common(struct irq_domain * domain,unsigned int virq,unsigned int nr_irqs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/hardirq.h>

void irq_enter(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/hardirq.h>

void irq_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern bool irq_fpu_usable(void);
bool irq_fpu_usable(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irqdomain.h>

void irq_set_default_host(struct irq_domain * domain)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jiffies.h>

unsigned long volatile __cacheline_aligned_in_smp __jiffy_arch_data jiffies;


extern void kernel_fpu_begin_mask(unsigned int kfpu_mask);
void kernel_fpu_begin_mask(unsigned int kfpu_mask)
{
	lx_emul_trace_and_stop(__func__);
}


extern void kernel_fpu_end(void);
void kernel_fpu_end(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void kfree_sensitive(const void * p)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void kfree_skb(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void kfree_skb_list(struct sk_buff * segs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void kmem_cache_destroy(struct kmem_cache * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timekeeping.h>

ktime_t ktime_get_coarse_with_offset(enum tk_offsets offs)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timekeeping.h>

void ktime_get_real_ts64(struct timespec64 * ts)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcutiny.h>

void kvfree(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void * kvmalloc_node(size_t size,gfp_t flags,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timer.h>

int mod_timer(struct timer_list * timer,unsigned long expires)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mutex.h>

void __sched mutex_lock(struct mutex * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mutex.h>

void __sched mutex_unlock(struct mutex * lock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

bool napi_complete_done(struct napi_struct * n,int work_done)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void napi_disable(struct napi_struct * n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void napi_enable(struct napi_struct * n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

gro_result_t napi_gro_receive(struct napi_struct * napi,struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

bool napi_schedule_prep(struct napi_struct * n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void netif_carrier_off(struct net_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

void netif_napi_add(struct net_device * dev,struct napi_struct * napi,int (* poll)(struct napi_struct *,int),int weight)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int nla_put(struct sk_buff * skb,int attrtype,int attrlen,const void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int nla_put_64bit(struct sk_buff * skb,int attrtype,int attrlen,const void * data,int padattr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/prandom.h>

u32 prandom_u32(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

int pskb_expand_head(struct sk_buff * skb,int nhead,int ntail,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * pskb_put(struct sk_buff * skb,struct sk_buff * tail,int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool queue_delayed_work_on(int cpu,struct workqueue_struct * wq,struct delayed_work * dwork,unsigned long delay)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

bool queue_work_on(int cpu,struct workqueue_struct * wq,struct work_struct * work)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/refcount.h>

void refcount_warn_saturate(refcount_t * r,enum refcount_saturation_type t)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int register_netdevice(struct net_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/random.h>

bool rng_is_initialized(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/rtnetlink.h>

void rtnl_link_unregister(struct rtnl_link_ops * ops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_lock(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rtnetlink.h>

void rtnl_unlock(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

signed long __sched schedule_timeout(signed long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

void setup_udp_tunnel_sock(struct net * net,struct socket * sock,struct udp_tunnel_sock_cfg * cfg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/scatterlist.h>

void sg_init_table(struct scatterlist * sgl,unsigned int nents)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/siphash.h>

u64 siphash_4u64(const u64 first,const u64 second,const u64 third,const u64 forth,const siphash_key_t * key)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_clear_memalloc(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/sock.h>

void sk_set_memalloc(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netdevice.h>

int skb_checksum_help(struct sk_buff * skb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

struct sk_buff * skb_clone(struct sk_buff * skb,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

int skb_copy_bits(const struct sk_buff * skb,int offset,void * to,int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

int skb_cow_data(struct sk_buff * skb,int tailbits,struct sk_buff ** trailer)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

struct sk_buff * skb_dequeue(struct sk_buff_head * list)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * skb_pull(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * skb_push(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * skb_put(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void skb_queue_purge(struct sk_buff_head * list)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void skb_queue_tail(struct sk_buff_head * list,struct sk_buff * newsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void skb_scrub_packet(struct sk_buff * skb,bool xnet)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

int skb_to_sgvec(struct sk_buff * skb,struct scatterlist * sg,int offset,int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void skb_trim(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/netdevice.h>

void synchronize_net(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/workqueue.h>

struct workqueue_struct *system_power_efficient_wq;


#include <net/udp_tunnel.h>

int udp_sock_create4(struct net * net,struct udp_port_cfg * cfg,struct socket ** sockp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

int udp_sock_create6(struct net * net,struct udp_port_cfg * cfg,struct socket ** sockp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

int udp_tunnel6_xmit_skb(struct dst_entry * dst,struct sock * sk,struct sk_buff * skb,struct net_device * dev,struct in6_addr * saddr,struct in6_addr * daddr,__u8 prio,__u8 ttl,__be32 label,__be16 src_port,__be16 dst_port,bool nocheck)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

void udp_tunnel_sock_release(struct socket * sock)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/udp_tunnel.h>

void udp_tunnel_xmit_skb(struct rtable * rt,struct sock * sk,struct sk_buff * skb,__be32 src,__be32 dst,__u8 tos,__u8 ttl,__be16 df,__be16 src_port,__be16 dst_port,bool xnet,bool nocheck)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/net_namespace.h>

void unregister_pernet_device(struct pernet_operations * ops)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwsem.h>

void up_read(struct rw_semaphore * sem)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rwsem.h>

void up_write(struct rw_semaphore * sem)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/random.h>

int wait_for_random_bytes(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void wq_worker_running(struct task_struct * task);
void wq_worker_running(struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}


extern void wq_worker_sleeping(struct task_struct * task);
void wq_worker_sleeping(struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/chacha20poly1305.h>

bool xchacha20poly1305_decrypt(u8 * dst,const u8 * src,const size_t src_len,const u8 * ad,const size_t ad_len,const u8 nonce[XCHACHA20POLY1305_NONCE_SIZE],const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}


#include <crypto/chacha20poly1305.h>

void xchacha20poly1305_encrypt(u8 * dst,const u8 * src,const size_t src_len,const u8 * ad,const size_t ad_len,const u8 nonce[XCHACHA20POLY1305_NONCE_SIZE],const u8 key[CHACHA20POLY1305_KEY_SIZE])
{
	lx_emul_trace_and_stop(__func__);
}

