/*
 * \brief  Stream id 0
 * \author Johannes Schlatow
 * \date   2021-08-04
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__STREAMS__STREAM0_H_
#define _CTF__STREAMS__STREAM0_H_

#include <base/fixed_stdint.h>

#include <ctf/events/rpc_name.h>
#include <ctf/event_header.h>

namespace Ctf {
	struct Rpc_call;
	struct Rpc_returned;
	struct Rpc_dispatch;
	struct Rpc_reply;
	struct Signal_submit;
	struct Signal_receive;
}

struct Ctf::Rpc_call : Ctf::Event_header<1>,
                       Ctf::Rpc_name
{
	Rpc_call(const char *name, size_t len)
	: Rpc_name(name, len)
	{ }
} __attribute__((packed));

struct Ctf::Rpc_returned : Ctf::Event_header<2>,
                           Ctf::Rpc_name
{
	Rpc_returned(const char *name, size_t len)
	: Rpc_name(name, len)
	{ }
} __attribute__((packed));

struct Ctf::Rpc_dispatch : Ctf::Event_header<3>,
                           Ctf::Rpc_name
{
	Rpc_dispatch(const char *name, size_t len)
	: Rpc_name(name, len)
	{ }
} __attribute__((packed));

struct Ctf::Rpc_reply : Ctf::Event_header<4>,
                        Ctf::Rpc_name
{
	Rpc_reply(const char *name, size_t len)
	: Rpc_name(name, len)
	{ }
} __attribute__((packed));

struct Ctf::Signal_submit  : Ctf::Event_header<5>
{
	uint32_t _number { };

	Signal_submit(uint32_t number)
	: _number(number)
	{ }
} __attribute__((packed));

struct Ctf::Signal_receive : Ctf::Event_header<6>
{
	uint32_t _number  { };
	uint64_t _context { };

	Signal_receive(uint32_t number, void* context)
	: _number(number),
	  _context((uint64_t)context)
	{ }
} __attribute__((packed));

#endif /* _CTF__STREAMS__STREAM0_H_ */
