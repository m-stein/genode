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

/* contrib linux includes */
#include <../drivers/net/wireguard/messages.h>

void print_hex(unsigned long x);


void glue_wg_setup(void) { }


void glue_wg_newlink(void) { }


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
