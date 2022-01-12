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
#include <../drivers/net/wireguard/messages.h>
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


void glue_wg_setup(void)
{
	wireguard_rtnl_link_ops()->setup(&_glue_wg_net_dev.public_data);
}


void glue_wg_newlink(void)
{
	wireguard_rtnl_link_ops()->newlink(
		&_glue_wg_src_net,
		&_glue_wg_net_dev.public_data,
		 _glue_wg_tb,
		 _glue_wg_data,
		&_glue_wg_extack);
}


void glue_wg_set_device_init(glue_uint16_t       listen_port,
                             glue_uint8_t const *private_key) { }


void glue_wg_open(void) { }


void glue_wg_set_device_peer(glue_uint8_t  const *public_key,
                             glue_uint8_t  const *endpoint_ip,
                             glue_uint16_t const  endpoint_port)
{
	unsigned idx;
	for (idx = 0; idx < NOISE_PUBLIC_KEY_LEN; idx++) {
		print_hex(public_key[idx]);
	}
	for (idx = 0; idx < 4; idx++) {
		print_hex(endpoint_ip[idx]);
	}
	print_hex(endpoint_port);
}
