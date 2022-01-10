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

void print_hex(int x);


void glue_wg_set_device(glue_uint16_t       listen_port,
                        glue_uint8_t const *private_key)
{
	unsigned idx;
	print_hex(listen_port);
	for (idx = 0; idx < NOISE_PUBLIC_KEY_LEN; idx++) {
		print_hex(private_key[idx]);
	}
}
