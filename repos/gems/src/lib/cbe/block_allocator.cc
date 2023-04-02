/*
 * \brief  Module for managing the block allocation
 * \author Josef Soentgen
 * \date   2023-02-28
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* local includes */
#include "block_allocator.h"

using namespace Genode;
using namespace Cbe;


Block_allocator_request::Block_allocator_request(unsigned long src_module_id,
                                                 unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, BLOCK_ALLOCATOR }
{ }


void Block_allocator_request::print(Genode::Output &out) const
{
	Genode::print(out, type_to_string(_type));
}


void Block_allocator_request::create(void             *buf_ptr,
                                     size_t            buf_size,
                                     Genode::uint64_t  src_module_id,
                                     Genode::uint64_t  src_request_id,
                                     Genode::size_t    req_type,
                                     void             *prim_ptr,
                                     Genode::size_t    prim_size)
{
	Block_allocator_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;

	if (prim_size > sizeof(req._prim)) {
		error(prim_size, " ", sizeof(req._prim));
		class Bad_size_1 { };
		throw Bad_size_1 { };
	}
	memcpy(&req._prim, prim_ptr, prim_size);

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


char const *Block_allocator_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case GET:     return "get";
	}
	return "?";
}


void Block_allocator::_execute_get(Channel &channel,
                                   bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		if (1 /* XXX overflow check */) {
			req._blk_nr = _first_block + _nr_of_blks;
			++_nr_of_blks;

			_mark_req_successful(channel, progress);
		} else
			_mark_req_failed(channel, progress, " get next block number");

		return;

	default:
		return;
	}
}


void Block_allocator::_mark_req_failed(Channel    &channel,
                                       bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Block_allocator::_mark_req_successful(Channel &channel,
                                           bool    &progress)
{
	channel._request._success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


bool Block_allocator::_peek_completed_request(Genode::uint8_t *buf_ptr,
                                              Genode::size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
			return true;
		}
	}
	return false;
}


void Block_allocator::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


Block_allocator::Block_allocator(uint64_t first_block)
:
	_first_block { first_block },
	_nr_of_blks  { 0 }
{ }


bool Block_allocator::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Block_allocator::submit_request(Module_request &req)
{
	for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *static_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Block_allocator::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		Request &req { channel._request };
		switch (req._type) {
		case Request::GET:

			if (channel._state == Channel::SUBMITTED)
				channel._state = Channel::PENDING;

			_execute_get(channel, progress);

			break;
		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}
