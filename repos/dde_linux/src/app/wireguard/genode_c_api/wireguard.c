/*
 * \brief  Glue code between Genode C++ code and Wireguard C code
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* app/wireguard includes */
#include <genode_c_api/wireguard.h>
#include <lx_emul.h>

/* contrib linux includes */
#include <../drivers/net/wireguard/device.h>
#include <../drivers/net/wireguard/messages.h>
#include <uapi/linux/wireguard.h>
#include <net/genetlink.h>


/*
 * The contrib code expects the private device data to be located directly
 + behind the public data with a certain alignment. It does pointer arithmetic
 * based on a pointer to the public data in order to determine the private
 * data's base.
 */
struct genode_wg_net_device
{
	struct net_device public_data;
	struct wg_device  private_data __attribute__((aligned(NETDEV_ALIGN)));
}
__attribute__((aligned(NETDEV_ALIGN), packed));


struct genode_wg_nlattr_ifname
{
	struct nlattr header;
	char          data[1] __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_private_key
{
	struct nlattr header;
	char          data[NOISE_PUBLIC_KEY_LEN] __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_public_key
{
	struct nlattr header;
	char          data[NOISE_PUBLIC_KEY_LEN] __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_symmetric_key
{
	struct nlattr header;
	char          data[NOISE_SYMMETRIC_KEY_LEN] __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_u8
{
	struct nlattr   header;
	genode_wg_u8_t  data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_u16
{
	struct nlattr   header;
	genode_wg_u16_t data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_u32
{
	struct nlattr   header;
	genode_wg_u32_t data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_u64
{
	struct nlattr   header;
	genode_wg_u64_t data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_in_addr
{
	struct nlattr  header;
	struct in_addr data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_sockaddr
{
	struct nlattr   header;
	struct sockaddr data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_kernel_timespec
{
	struct nlattr            header;
	struct __kernel_timespec data __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_allowed_ip
{
	struct nlattr header;
	struct genode_wg_nlattr_u16     family    __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_in_addr ipaddr    __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_u8      cidr_mask __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_allowed_ips
{
	struct nlattr header;
	struct genode_wg_nlattr_allowed_ip ip_0 __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_peer
{
	struct nlattr                           header;
	struct genode_wg_nlattr_public_key      public_key          __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_symmetric_key   preshared_key       __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_u32             flags               __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_sockaddr        endpoint            __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_u16             pki                 __attribute__((aligned(NLA_ALIGNTO))); /* persistent keepalive internal */
	struct genode_wg_nlattr_kernel_timespec last_handshake_time __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_u64             rx_bytes            __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_u64             tx_bytes            __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_allowed_ips     allowed_ips         __attribute__((aligned(NLA_ALIGNTO)));
	struct genode_wg_nlattr_u32             protocol_version    __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


struct genode_wg_nlattr_peers
{
	struct nlattr                header;
	struct genode_wg_nlattr_peer peer_0 __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO), packed));


static struct genode_wg_net_device  _genode_wg_net_dev;
static struct net                   _genode_wg_src_net;
static struct nlattr               *_genode_wg_tb[1];
static struct nlattr               *_genode_wg_data[1];
static struct netlink_ext_ack       _genode_wg_extack;
static struct rtnl_link_ops        *_genode_wg_rtnl_link_ops;
static struct genl_family          *_genode_wg_genl_family;
static struct socket                _genode_wg_socket;

/* for the call to wg_set_device that installs listen port and private key */
static struct sock                  _genode_wg_sock;
static struct sk_buff               _genode_wg_sk_buff;


void genode_wg_rtnl_link_ops(struct rtnl_link_ops *ops)
{
	_genode_wg_rtnl_link_ops = ops;
}


void genode_wg_genl_family(struct genl_family * family)
{
	_genode_wg_genl_family = family;
}


struct net_device * genode_wg_net_device(void)
{
	return &_genode_wg_net_dev.public_data;
}


/**
 * Call 'wg_set_device' in the contrib code
 */
static void
_genode_wg_set_device(struct genl_info *info)
{
	unsigned idx;
	bool     op_not_found = true;

	for (idx = 0; idx < _genode_wg_genl_family->n_ops; idx++) {
		if (_genode_wg_genl_family->ops[idx].cmd == WG_CMD_SET_DEVICE) {
			_genode_wg_genl_family->ops[idx].doit(&_genode_wg_sk_buff, info);
			op_not_found = false;
		}
	}
	if (op_not_found) {
		printk("Error: cannot find op WG_CMD_SET_DEVICE\n");
		while (1) { }
	}
}


static void
_genode_wg_config_add_dev(genode_wg_u16_t              listen_port,
                          const genode_wg_u8_t * const priv_key)
{
	static unsigned called = 0;
	if (called++) {
		printk("%s re-called. Reconfiguration not supported yet\n", __func__);
		return;
	}

	/* prepare environment for the execution of 'wg_set_device' */
	_genode_wg_net_dev.public_data.rtnl_link_ops = _genode_wg_rtnl_link_ops;
	_genode_wg_sk_buff.sk = &_genode_wg_sock;

	{
		struct genode_wg_nlattr_ifname      ifname;
		struct genode_wg_nlattr_u16         port;
		struct genode_wg_nlattr_private_key private_key;
		struct nlattr *                     attrs[__WGDEVICE_A_LAST];
		struct genl_info                    info;

		ifname.data[0]      = '\0';
		ifname.header.nla_len = sizeof(ifname);

		port.data = listen_port;
		port.header.nla_len = sizeof(port);

		memcpy(private_key.data, priv_key, sizeof(private_key.data));

		private_key.header.nla_len = sizeof(private_key);

		memset(attrs, 0, sizeof(attrs));
		attrs[WGDEVICE_A_IFNAME]      = &ifname.header;
		attrs[WGDEVICE_A_LISTEN_PORT] = &port.header;
		attrs[WGDEVICE_A_PRIVATE_KEY] = &private_key.header;

		info.attrs = attrs;
		_genode_wg_set_device(&info);
	}

	/* prepare environment for the execution of 'wg_open' */
	_genode_wg_socket.sk = &_genode_wg_sock;

	/* trigger execution of 'wg_open' */
	_genode_wg_net_dev.public_data.netdev_ops->ndo_open(
		&_genode_wg_net_dev.public_data);
}


static void _genode_wg_config_rm_dev(genode_wg_u16_t listen_port)
{
	printk("%s not yet implemented\n", __func__);
}


static void
_genode_wg_config_add_peer(genode_wg_u16_t              listen_port,
                           genode_wg_u8_t const         endpoint_ip[4],
                           genode_wg_u16_t              endpoint_port,
                           genode_wg_u8_t const *const  pub_key,
                           genode_wg_u8_t const         allowed_ip_addr[4],
                           genode_wg_u8_t const         allowed_ip_prefix_length)
{
	struct genode_wg_nlattr_ifname ifname;
	struct genode_wg_nlattr_peers peers;
	struct nlattr *attrs[__WGDEVICE_A_LAST];
	struct genl_info info;
	struct genode_wg_nlattr_peer *peer = &peers.peer_0;

	ifname.data[0] = '\0';
	ifname.header.nla_len = sizeof(ifname);

	memset(&peers, 0, sizeof(peers));

	peers.header.nla_len = sizeof(peers);

	peer->header.nla_len = sizeof(peers.peer_0);
	peer->header.nla_type |= NLA_F_NESTED;
	peer->public_key.header.nla_len          = sizeof(peer->public_key);
	peer->preshared_key.header.nla_len       = sizeof(peer->preshared_key);
	peer->flags.header.nla_len               = sizeof(peer->flags);
	peer->endpoint.header.nla_len            = sizeof(peer->endpoint);
	peer->pki.header.nla_len                 = sizeof(peer->pki);
	peer->last_handshake_time.header.nla_len = sizeof(peer->last_handshake_time);
	peer->rx_bytes.header.nla_len            = sizeof(peer->rx_bytes);
	peer->tx_bytes.header.nla_len            = sizeof(peer->tx_bytes);
	peer->allowed_ips.header.nla_len         = sizeof(peer->allowed_ips);
	peer->protocol_version.header.nla_len    = sizeof(peer->protocol_version);

	memcpy(peer->public_key.data, pub_key, sizeof(peer->public_key.data));

	memset(attrs, 0, sizeof(attrs));
	attrs[WGDEVICE_A_IFNAME] = &ifname.header;
	attrs[WGDEVICE_A_PEERS]  = &peers.header;

	info.attrs = attrs;
	_genode_wg_set_device(&info);
}


static void
_genode_wg_config_rm_peer(genode_wg_u16_t      listen_port,
                          genode_wg_u8_t const endpoint_ip[4],
                          genode_wg_u16_t      endpoint_port)
{
	printk("%s not yet implemented\n", __func__);
}


static struct genode_wg_config_callbacks _config_callbacks = {
	.add_device    = _genode_wg_config_add_dev,
	.remove_device = _genode_wg_config_rm_dev,
	.add_peer      = _genode_wg_config_add_peer,
	.remove_peer   = _genode_wg_config_rm_peer
};


void genode_wg_notify_peers(void)
{
}


static int user_task_function(void *arg)
{
	for (;;) {
		genode_wg_update_config(&_config_callbacks);

		/* block until lx_emul_task_unblock */
		lx_emul_task_schedule(true);
	}
	return 0;
}


static struct task_struct * _user_task_struct_ptr = NULL;


void lx_user_handle_io(void)
{
	if (_user_task_struct_ptr)
		lx_emul_task_unblock(_user_task_struct_ptr);
}


void lx_user_init(void)
{
	pid_t pid;

	/* trigger execution of 'wg_setup' */
	_genode_wg_rtnl_link_ops->setup(&_genode_wg_net_dev.public_data);

	/* trigger execution of 'wg_newlink' */
	_genode_wg_rtnl_link_ops->newlink(
		&_genode_wg_src_net,
		&_genode_wg_net_dev.public_data,
		 _genode_wg_tb,
		 _genode_wg_data,
		&_genode_wg_extack);

	/* create user task, which handles network traffic and configuration changes */
	pid = kernel_thread(user_task_function, NULL, CLONE_FS | CLONE_FILES);
	_user_task_struct_ptr = find_task_by_pid_ns(pid, NULL);
}
