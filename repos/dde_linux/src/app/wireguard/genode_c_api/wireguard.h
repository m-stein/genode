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

#ifndef _GENODE_C_API__WIREGUARD_H_
#define _GENODE_C_API__WIREGUARD_H_

enum { GENODE_WG_KEY_LEN = 32 };

typedef unsigned char      genode_wg_u8_t;
typedef unsigned short     genode_wg_u16_t;
typedef unsigned int       genode_wg_u32_t;
typedef unsigned long long genode_wg_u64_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*genode_wg_config_add_dev_t)
	(genode_wg_u16_t listen_port, const genode_wg_u8_t * const priv_key);

typedef void (*genode_wg_config_rm_dev_t) (genode_wg_u16_t listen_port);

typedef void (*genode_wg_config_add_peer_t)
	(genode_wg_u16_t listen_port, genode_wg_u8_t const endpoint_ip[4],
	 genode_wg_u16_t endpoint_port, const genode_wg_u8_t * const pub_key);

typedef void (*genode_wg_config_rm_peer_t)
	(genode_wg_u16_t listen_port, genode_wg_u8_t const endpoint_ip[4],
	 genode_wg_u16_t endpoint_port);

typedef void (*genode_wg_config_add_route_t)
	(genode_wg_u16_t listen_port, genode_wg_u8_t const endpoint_ip[4],
	 genode_wg_u16_t endpoint_port, genode_wg_u8_t const allowed_ip_addr[4],
	 genode_wg_u8_t const allowed_ip_subnet_mask[4]);

typedef void (*genode_wg_config_rm_route_t)
	(genode_wg_u16_t listen_port, genode_wg_u8_t const endpoint_ip[4],
	 genode_wg_u16_t endpoint_port, genode_wg_u8_t const allowed_ip_addr[4]);


struct genode_wg_config_callbacks
{
	genode_wg_config_add_dev_t   add_device;
	genode_wg_config_rm_dev_t    remove_device;
	genode_wg_config_add_peer_t  add_peer;
	genode_wg_config_rm_peer_t   remove_peer;
	genode_wg_config_add_route_t add_route;
	genode_wg_config_rm_route_t  remove_route;
};


void genode_wg_update_config(struct genode_wg_config_callbacks * callbacks);

void genode_wg_notify_peers(void);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__WIREGUARD_H_ */
