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
#include <glue.h>
#include <lx_emul.h>

/* contrib linux includes */
#include <../drivers/net/wireguard/device.h>

/*
 * The contrib code expects the private device data to be located directly
 + behind the public data with a certain alignment. It does pointer arithmetic
 * based on a pointer to the public data in order to determine the private
 * data's base.
 */
struct wg_net_device
{
	struct net_device public_data;
	struct wg_device  private_data __attribute__((aligned(NETDEV_ALIGN)));
}
__attribute__((aligned(NETDEV_ALIGN)));

static struct wg_net_device   _glue_wg_net_dev;
static struct net             _glue_wg_src_net;
static struct nlattr         *_glue_wg_tb[1];
static struct nlattr         *_glue_wg_data[1];
static struct netlink_ext_ack _glue_wg_extack;


void print_hex(unsigned long x);


static struct rtnl_link_ops *_wireguard_rtnl_link_ops;


void glue_wg_rtnl_link_ops(struct rtnl_link_ops * ops)
{
	_wireguard_rtnl_link_ops = ops;
}


void glue_wg_setup(void)
{
	_wireguard_rtnl_link_ops->setup(&_glue_wg_net_dev.public_data);
}


void glue_wg_newlink(void)
{
	_wireguard_rtnl_link_ops->newlink(
		&_glue_wg_src_net,
		&_glue_wg_net_dev.public_data,
		 _glue_wg_tb,
		 _glue_wg_data,
		&_glue_wg_extack);
}


void glue_wg_set_device_init(glue_uint16_t      listen_port,
                             glue_uint8_t const private_key[GLUE_KEY_LEN]) { }


void glue_wg_open(void) { }


void glue_wg_set_device_peer(glue_uint8_t  const public_key[GLUE_KEY_LEN],
                             glue_uint8_t  const endpoint_ip[4],
                             glue_uint16_t       endpoint_port,
                             glue_uint8_t  const allowed_ip_addr[4],
                             glue_uint8_t  const allowed_ip_subnet_mask[4])
{
	unsigned idx;
	printk("public key\n");
	for (idx = 0; idx < GLUE_KEY_LEN; idx += 4) {
		printk("  %d: %x %x %x %x\n", idx, public_key[idx + 0], public_key[idx + 1], public_key[idx + 2], public_key[idx + 3]);
	}
	printk("endpoint ip %x %x %x %x port %x\n", endpoint_ip[0], endpoint_ip[1], endpoint_ip[2], endpoint_ip[3], endpoint_port);
	printk("allowed ip %x %x %x %x subnet mask %x %x %x %X\n", allowed_ip_addr[0], allowed_ip_addr[1], allowed_ip_addr[2], allowed_ip_addr[3], allowed_ip_subnet_mask[0], allowed_ip_subnet_mask[1], allowed_ip_subnet_mask[2], allowed_ip_subnet_mask[3]);
}
