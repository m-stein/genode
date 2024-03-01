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
#include <tresor/request_scheduler.h>

using namespace Tresor;

Request::Request(Operation op, Virtual_byte_range virt_range, Request_offset offset, Number_of_blocks num_blocks, Request_tag tag, Generation &gen)
:
	_op(op), _virt_range(virt_range), _num_blocks(num_blocks), _offset(offset), _tag(tag), _gen(gen), _helper(*this)
{ }


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
