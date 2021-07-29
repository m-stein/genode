/*
 * \brief  Trace event for logging RPC names
 * \author Johannes Schlatow
 * \date   2021-08-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__EVENTS__RPC_NAME_H_
#define _CTF__EVENTS__RPC_NAME_H_

#include <util/string.h>

namespace Ctf {
	struct Rpc_name;
}

struct Ctf::Rpc_name
{
	char _name[0];

	Rpc_name(const char *name, size_t len) {
		Genode::copy_cstring(_name, name, len); }
} __attribute__((packed));

#endif /* _CTF__EVENTS__RPC_NAME_H_ */
