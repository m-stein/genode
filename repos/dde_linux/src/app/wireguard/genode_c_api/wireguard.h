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

typedef unsigned char  genode_wg_u8;
typedef unsigned short genode_wg_u16;

#ifdef __cplusplus
extern "C" {
#endif

void
genode_wg_initialize_driver(genode_wg_u16 listen_port,
                            const char * const private_key_buf);

void
genode_wg_set_peer_config(genode_wg_u8 const public_key[GENODE_WG_KEY_LEN],
                          genode_wg_u8 const endpoint_ip[4],
                          genode_wg_u16      endpoint_port,
                          genode_wg_u8 const allowed_ip_addr[4],
                          genode_wg_u8 const allowed_ip_subnet_mask[4]);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__WIREGUARD_H_ */
