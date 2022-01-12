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

#ifndef _GLUE_CPP_H_
#define _GLUE_CPP_H_

/* app/wireguard includes */
#include <glue.h>

extern "C" void glue_wg_setup();

extern "C" void glue_wg_newlink();

extern "C" void glue_wg_set_device_init(glue_uint16_t      listen_port,
                                        glue_uint8_t const private_key[GLUE_KEY_LEN]);

extern "C" void glue_wg_open();

extern "C" void glue_wg_set_device_peer(glue_uint8_t  const public_key[GLUE_KEY_LEN],
                                        glue_uint8_t  const endpoint_ip[4],
                                        glue_uint16_t       endpoint_port,
                                        glue_uint8_t  const allowed_ip_addr[4],
                                        glue_uint8_t  const allowed_ip_subnet_mask[4]);

#endif /* _GLUE_CPP_H_ */
