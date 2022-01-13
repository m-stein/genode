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
__attribute__((aligned(NETDEV_ALIGN)));


struct genode_wg_nlattr_ifname
{
	struct nlattr header;
	char          data[1] __attribute__((aligned(NLA_ALIGNTO)));
}
__attribute__((aligned(NLA_ALIGNTO)));


static struct genode_wg_net_device  _genode_wg_net_dev;
static struct net                   _genode_wg_src_net;
static struct nlattr               *_genode_wg_tb[1];
static struct nlattr               *_genode_wg_data[1];
static struct netlink_ext_ack       _genode_wg_extack;
static struct rtnl_link_ops        *_genode_wg_rtnl_link_ops;
static struct genl_family          *_genode_wg_genl_family;

/* for the call to wg_set_device that installs listen port and private key */
static struct sock                     _genode_wg_sock;
static struct sk_buff                  _genode_wg_sk_buff;
static struct genl_info                _genode_wg_genl_info;
static struct nlattr                  *_genode_wg_genl_info_attrs[__WGDEVICE_A_LAST];
static struct genode_wg_nlattr_ifname  _genode_wg_nlattr_ifname;


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


void
genode_wg_initialize_driver(genode_wg_u16      listen_port,
                            const char * const private_key_buf)
{
	unsigned idx;
	bool found_wg_cmd = false;

	/* trigger execution of 'wg_setup' */
	_genode_wg_rtnl_link_ops->setup(&_genode_wg_net_dev.public_data);

	/* trigger execution of 'wg_newlink' */
	_genode_wg_rtnl_link_ops->newlink(
		&_genode_wg_src_net,
		&_genode_wg_net_dev.public_data,
		 _genode_wg_tb,
		 _genode_wg_data,
		&_genode_wg_extack);

	/* prepare environment for the execution of 'wg_set_device' */
	_genode_wg_net_dev.public_data.rtnl_link_ops = _genode_wg_rtnl_link_ops;
	_genode_wg_sk_buff.sk = &_genode_wg_sock;
	_genode_wg_genl_info.attrs = _genode_wg_genl_info_attrs;
	_genode_wg_nlattr_ifname.data[0] = '\0';
	_genode_wg_genl_info_attrs[WGDEVICE_A_IFNAME] = &_genode_wg_nlattr_ifname.header;

	/*
	 * Trigger execution of 'wg_set_device' in order to install listen port
	 * and private key.
	 */
	for (idx = 0; idx < _genode_wg_genl_family->n_ops; idx++) {
		if (_genode_wg_genl_family->ops[idx].cmd == WG_CMD_SET_DEVICE) {
			_genode_wg_genl_family->ops[idx].doit(&_genode_wg_sk_buff, &_genode_wg_genl_info);
			found_wg_cmd = true;
		}
	}
	if (!found_wg_cmd) {
		printk("Error: cannot find op WG_CMD_SET_DEVICE\n");
		while (1) { }
	}
	/* trigger execution of 'wg_open' */
	_genode_wg_net_dev.public_data.netdev_ops->ndo_open(
		&_genode_wg_net_dev.public_data);
}



void
genode_wg_set_peer_config(genode_wg_u8 const public_key[GENODE_WG_KEY_LEN],
                          genode_wg_u8 const endpoint_ip[4],
                          genode_wg_u16      endpoint_port,
                          genode_wg_u8 const allowed_ip_addr[4],
                          genode_wg_u8 const allowed_ip_subnet_mask[4])
{
	unsigned idx;
	printk("public key\n");
	for (idx = 0; idx < GENODE_WG_KEY_LEN; idx += 4) {
		printk("  %d: %x %x %x %x\n", idx, public_key[idx + 0], public_key[idx + 1], public_key[idx + 2], public_key[idx + 3]);
	}
	printk("endpoint ip %x %x %x %x port %x\n", endpoint_ip[0], endpoint_ip[1], endpoint_ip[2], endpoint_ip[3], endpoint_port);
	printk("allowed ip %x %x %x %x subnet mask %x %x %x %X\n", allowed_ip_addr[0], allowed_ip_addr[1], allowed_ip_addr[2], allowed_ip_addr[3], allowed_ip_subnet_mask[0], allowed_ip_subnet_mask[1], allowed_ip_subnet_mask[2], allowed_ip_subnet_mask[3]);

	// wg_set_device(...);
}
