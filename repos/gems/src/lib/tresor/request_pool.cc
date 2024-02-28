/*
 * \brief  Module for scheduling requests for processing
 * \author Martin Stein
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/request_pool.h>

using namespace Tresor;

Request::Request(Operation op, Virtual_block_address vba,
                 Request_offset offset, Number_of_blocks count, Request_tag tag, Generation &gen)
:
	_op(op), _vba(vba), _offset(offset), _count(count), _tag(tag), _gen(gen), _helper(*this)
{ }


void Request::print(Output &out) const
{
	Genode::print(out, op_to_string(_op));
	switch (_op) {
	case READ:
	case WRITE:
	case SYNC:
		if (_count > 1)
			Genode::print(out, " vbas ", _vba, "..", _vba + _count - 1);
		else
			Genode::print(out, " vba ", _vba);
		break;
	default: break;
	}
}


char const *Request::op_to_string(Operation op)
{
	switch (op) {
	case Request::READ: return "read";
	case Request::WRITE: return "write";
	case Request::SYNC: return "sync";
	case Request::CREATE_SNAPSHOT: return "create_snapshot";
	case Request::DISCARD_SNAPSHOT: return "discard_snapshot";
	case Request::REKEY: return "rekey";
	case Request::EXTEND_VBD: return "extend_vbd";
	case Request::EXTEND_FT: return "extend_ft";
	case Request::RESUME_REKEYING: return "resume_rekeying";
	case Request::DEINITIALIZE: return "deinitialize";
	case Request::INITIALIZE: return "initialize";
	}
	ASSERT_NEVER_REACHED;
}
