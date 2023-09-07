/*
 * \brief  Module for splitting unaligned/uneven I/O requests
 * \author Josef Soentgen
 * \date   2023-09-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include "splitter.h"


void *Tresor::Splitter_channel::_calculate_data_ptr(Virtual_block_address vba)
{
	ASSERT(state() == State::REQUEST);

	Splitter_request &req = *_req_ptr;

	Virtual_block_address const start_vba = req._offset / Tresor::BLOCK_SIZE;
	size_t const buffer_offset = (vba - start_vba) * Tresor::BLOCK_SIZE;

	return _req_ptr->_buffer_start + _offset + buffer_offset;
}


void Tresor::Splitter_channel::_handle_io(bool &progress)
{
	ASSERT(state() != State::IDLE);

	Splitter_request &req = *_req_ptr;

	switch (_state) {
	case State::PENDING:
	{
		using TRO = Tresor::Request::Operation;
		using SRO = Splitter_request::Operation;

		TRO const op = req._op == SRO::READ ? TRO::READ
		                                    : TRO::WRITE;

		generate_req<Tresor::Request>(State::COMPLETE,
			progress, op, req._success, _vba,
			0 /* offset */, _count, req._key_id, (uint32_t)id(),
			*const_cast<Tresor::Generation*>(&req._gen));

		state(State::REQUEST);
		break;
	}

	case State::PRE_REQUEST_PENDING:

		generate_req<Tresor::Request>(State::COMPLETE,
			progress, Tresor::Request::READ, req._success, _vba,
			0 /* offset */, 1, req._key_id, (uint32_t)id(),
			*const_cast<Tresor::Generation*>(&req._gen));

		state(State::PRE_REQUEST);
		break;

	case State::PRE_REQUEST_WRITE_PENDING:

		generate_req<Tresor::Request>(State::COMPLETE,
			progress, Tresor::Request::WRITE, req._success, _vba,
			0 /* offset */, 1, req._key_id, (uint32_t)id(),
			*const_cast<Tresor::Generation*>(&req._gen));

		state(State::PRE_REQUEST_WRITE);
		break;

	case State::POST_REQUEST_PENDING:

		generate_req<Tresor::Request>(State::COMPLETE,
			progress, Tresor::Request::READ, req._success, _vba,
			0 /* offset */, 1, req._key_id, (uint32_t)id(),
			*const_cast<Tresor::Generation*>(&req._gen));

		state(State::POST_REQUEST);
		break;

	case State::POST_REQUEST_WRITE_PENDING:

		generate_req<Tresor::Request>(State::COMPLETE,
			progress, Tresor::Request::WRITE, req._success, _vba,
			0 /* offset */, 1, req._key_id, (uint32_t)id(),
			*const_cast<Tresor::Generation*>(&req._gen));

		state(State::POST_REQUEST_WRITE);
		break;

	/* not required here */
	case State::REQUEST:            [[fallthrough]];
	case State::PRE_REQUEST:        [[fallthrough]];
	case State::PRE_REQUEST_WRITE:  [[fallthrough]];
	case State::POST_REQUEST:       [[fallthrough]];
	case State::POST_REQUEST_WRITE: [[fallthrough]];
	case State::COMPLETE:           [[fallthrough]];
	case State::IDLE:
		break;
	}
}


void Tresor::Splitter_channel::_generated_req_completed(State_uint state_uint)
{
	ASSERT(state_uint == State::COMPLETE);

	Splitter_request &req = *_req_ptr;

	bool const read = req._op == Splitter_request::Operation::READ;

	switch (_state) {
	case State::REQUEST:
	{
		Genode::uint64_t const bytes = _count * Tresor::BLOCK_SIZE;
		_total_bytes += bytes;
		_offset      += bytes;
		break;
	}
	case State::PRE_REQUEST:
	{
		Genode::uint64_t const block_offset = req._offset % Tresor::BLOCK_SIZE;
		Genode::uint64_t const block_bytes  = Tresor::BLOCK_SIZE - block_offset;
		size_t const copy_length = Genode::min(block_bytes, req._buffer_num_bytes);

		_total_bytes += copy_length;
		_offset      += copy_length;

		if (read) {
			Genode::memcpy((void*)req._buffer_start,
			               (char*)&_block_data + block_offset,
			               copy_length);
		} else {
			Genode::memcpy((char*)&_block_data + block_offset,
			               (void*)req._buffer_start, copy_length);

			/* leave here as we have to write the block back first */
			state(State::PRE_REQUEST_WRITE_PENDING);
			return;
		}

		break;
	}
	case State::POST_REQUEST:
	{
		Genode::uint64_t const copy_length = req._buffer_num_bytes - _total_bytes;

		_total_bytes += copy_length;
		_offset      += copy_length;

		if (read) {
			Genode::memcpy((char*)req._buffer_start + _offset,
			               (void*)&_block_data,
			               copy_length);
		} else {
			Genode::memcpy((void*)&_block_data,
			               (char*)req._buffer_start + _offset, copy_length);

			/* leave here as we have to write the block back first */
			state(State::POST_REQUEST_WRITE_PENDING);
			return;
		}

		break;
	}
	/* not required here */
	case State::PENDING:                    [[fallthrough]];
	case State::PRE_REQUEST_PENDING:        [[fallthrough]];
	case State::PRE_REQUEST_WRITE_PENDING:  [[fallthrough]];
	case State::PRE_REQUEST_WRITE:          [[fallthrough]];
	case State::POST_REQUEST_PENDING:       [[fallthrough]];
	case State::POST_REQUEST_WRITE_PENDING: [[fallthrough]];
	case State::POST_REQUEST_WRITE:         [[fallthrough]];
	case State::COMPLETE:                   [[fallthrough]];
	case State::IDLE:
		break;
	}

	/* we are done */
	if (_total_bytes == req._buffer_num_bytes) {
		state(State::COMPLETE);
		return;
	}

	/* XXX consolidate below with _request_submitted */

	Genode::uint64_t const offset = req._offset + _offset;
	size_t const left = req._buffer_num_bytes - _total_bytes;

	bool const unaligned = (offset % Tresor::BLOCK_SIZE) != 0;
	if (unaligned) {
		_vba = offset / Tresor::BLOCK_SIZE;

		state(Splitter_channel::PRE_REQUEST_PENDING);
		return;
	}

	_vba   = _offset / Tresor::BLOCK_SIZE;
	_count = (uint32_t)left / Tresor::BLOCK_SIZE;

	bool const uneven = (left % Tresor::BLOCK_SIZE) != 0;
	if (!_count && uneven) {
		_count = 1;
		state(Splitter_channel::POST_REQUEST_PENDING);
		return;
	}

	state(Splitter_channel::PENDING);
}


void Tresor::Splitter_channel::_request_submitted(Module_request &module_req)
{
	_reset();

	_req_ptr = static_cast<Splitter_request*>(&module_req);

	Splitter_request &req = *_req_ptr;


	/*
	 * Prepare the request depending on the given characteristics,
	 * e.g, if it is unaligned and/or uneven.
	 *
	 * Requests that do not start at a BLOCK_SIZE boundary are handled
	 * first where the unaligned bytes from the containing block will be
	 * read and mixed with the buffer.
	 */
	bool const unaligned = (req._offset % Tresor::BLOCK_SIZE) != 0;
	if (unaligned) {
		_vba = req._offset / Tresor::BLOCK_SIZE;

		state(Splitter_channel::PRE_REQUEST_PENDING);
		return;
	}

	_vba   = req._offset / Tresor::BLOCK_SIZE;
	_count = (uint32_t)req._buffer_num_bytes / Tresor::BLOCK_SIZE;

	bool const uneven = (req._buffer_num_bytes % Tresor::BLOCK_SIZE) != 0;
	if (!_count && uneven) {
		_count = 1;
		state(Splitter_channel::POST_REQUEST_PENDING);
		return;
	}

	state(Splitter_channel::PENDING);
}


bool Tresor::Splitter_channel::_request_complete()
{
	return state() == State::COMPLETE;
}


void Tresor::Splitter_channel::execute(bool &progress)
{
	if (state() == State::IDLE)
		return;

	_handle_io(progress);
}


void *Tresor::Splitter_channel::query_data(Virtual_block_address vba)
{
	switch (_state) {
	/*
	 * A normal request might cover multiple blocks while
	 * PRE and POST correspond to exactly one.
	 */
	case State::REQUEST:
		return _calculate_data_ptr(vba);

	/*
	 * Always use the same temporary block for every
	 * lopsided request as each step is performed in
	 * sequence.
	 */
	case State::PRE_REQUEST:        [[fallthrough]];
	case State::PRE_REQUEST_WRITE:  [[fallthrough]];
	case State::POST_REQUEST:       [[fallthrough]];
	case State::POST_REQUEST_WRITE:
		return _vba == vba ? (void*)&_block_data : nullptr;
	default:
		break;
	}

	struct Invalid_state_for_query_data { };
	throw Invalid_state_for_query_data();

	return nullptr;
}


Tresor::Splitter::Splitter()
{
	for (Module_channel_id id = 0; id < NUM_CHANNELS; id++) {
		_channels[id].construct(id);
		add_channel(*_channels[id]);
	}
}


void Tresor::Splitter::execute(bool &progress)
{
	for_each_channel<Splitter_channel>([&] (Splitter_channel &chan) {
		chan.execute(progress);
	});
}
