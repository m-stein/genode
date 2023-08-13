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

char const *Request::op_to_string(Operation op)
{
	switch (op) {
	case Request::INVALID: return "invalid";
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


void Request_pool::_gen_superblock_control_req(Channel &chan, Channel_index chan_idx, bool &progress,
                                               Superblock_control_request::Type type, Virtual_block_address vba,
                                               Channel::State complete_state)
{
	chan._state = Channel::REQ_GENERATED;
	chan.generate_req<Superblock_control_request>(
		complete_state, progress, REQUEST_POOL, chan_idx, type, chan._req._offset, chan._req._tag,
		chan._req._count, vba, chan._generated_req_success, chan._request_finished,
		chan._sb_state, chan._req._gen);
}


void Request_pool::_execute_access_vbas(Channel &chan, Channel_index chan_idx, bool &progress, Superblock_control_request::Type type)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		_gen_superblock_control_req(
			chan, chan_idx, progress, type, chan._req._vba + chan._num_blks,
			Channel::ACCESS_VBA_AT_SB_CTRL_SUCCEEDED);
		break;

	case Channel::ACCESS_VBA_AT_SB_CTRL_SUCCEEDED:

		if (++chan._num_blks < chan._req._count)
			_gen_superblock_control_req(
				chan, chan_idx, progress, type, chan._req._vba + chan._num_blks,
				Channel::ACCESS_VBA_AT_SB_CTRL_SUCCEEDED);
		else
			_mark_req_successful(chan, chan_idx, progress);
		break;

	default: break;
	}
}


void Request_pool::_mark_req_failed(Channel &chan, bool &progress, unsigned long line)
{
	error("request_pool: request (", chan._req, ") failed at line ", line);
	chan._req._success = false;
	chan._state = Channel::REQ_COMPLETE;
	progress = true;
}


bool Request_pool::_handle_failed_generated_req(Channel &chan, Channel_index chan_idx, bool &progress, unsigned long line)
{
	if (chan._generated_req_success)
		return false;

	_mark_req_failed(chan, progress, line);
	_chan_idx_queue.dequeue(chan_idx);
	return true;
}


void Request_pool::_mark_req_successful(Channel &chan, Channel_index chan_idx, bool &progress)
{
	chan._req._success = true;
	chan._state = Channel::REQ_COMPLETE;
	_chan_idx_queue.dequeue(chan_idx);
	progress = true;
}


void Request_pool::_try_prepone_requests(Channel &chan, Channel_index chan_idx, bool &progress)
{
	bool requests_preponed { false };
	bool at_req_that_cannot_be_preponed { false };
	chan._num_requests_preponed = 0;
	while (chan._num_requests_preponed < MAX_NUM_REQUESTS_PREPONED &&
	       !at_req_that_cannot_be_preponed &&
	       !_chan_idx_queue.is_tail(chan_idx)) {

		switch (_channels[_chan_idx_queue.next(chan_idx)]._req._op) {
		case Request::READ:
		case Request::WRITE:
		case Request::SYNC:
		case Request::DISCARD_SNAPSHOT:

			_chan_idx_queue.move_one_slot_towards_tail(chan_idx);
			chan._num_requests_preponed++;
			requests_preponed = true;
			progress = true;
			break;

		default:

			at_req_that_cannot_be_preponed = true;
			break;
		}
	}
	if (!requests_preponed) {
		chan._state = Channel::PREPONED_REQUESTS_COMPLETE;
		progress = true;
	}
}


void Request_pool::_execute_extend_tree(Channel &chan, Channel_index chan_idx, Superblock_control_request::Type type, bool &progress)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		_gen_superblock_control_req(
			chan, chan_idx, progress, type, 0, Channel::TREE_EXTENSION_STEP_SUCCEEDED);
		break;

	case Channel::TREE_EXTENSION_STEP_SUCCEEDED:

		if (chan._request_finished)
			_mark_req_successful(chan, chan_idx, progress);
		else
			_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::PREPONED_REQUESTS_COMPLETE:

		_gen_superblock_control_req(
			chan, chan_idx, progress, type, 0, Channel::TREE_EXTENSION_STEP_SUCCEEDED);
		break;

	default: break;
	}
}


void Request_pool::_execute_rekey(Channel &chan, Channel_index chan_idx, bool &progress)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		_gen_superblock_control_req(
			chan, chan_idx, progress, Superblock_control_request::INITIALIZE_REKEYING,
			0, Channel::REKEY_INIT_SUCCEEDED);
		break;

	case Channel::REQ_RESUMED:
	case Channel::REKEY_INIT_SUCCEEDED:

		_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::REKEY_VBA_SUCCEEDED:

		if (chan._request_finished)
			_mark_req_successful(chan, chan_idx, progress);
		else
			_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::PREPONED_REQUESTS_COMPLETE:

		_gen_superblock_control_req(
			chan, chan_idx, progress, Superblock_control_request::REKEY_VBA,
			0, Channel::REKEY_VBA_SUCCEEDED);
		break;

	default: break;
	}
}


void Request_pool::_resume_request(Channel &chan, Channel_index chan_idx, bool &progress, Request::Operation op)
{
	chan._state = Channel::REQ_RESUMED;
	chan._req = Request { op };
	_chan_idx_queue.enqueue(chan_idx);
	progress = true;
}


void Request_pool::_execute_initialize(Channel &chan, Channel_index chan_idx, bool &progress)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		chan._vba = 0;
		_gen_superblock_control_req(
			chan, chan_idx, progress, Superblock_control_request::INITIALIZE,
			0, Channel::INITIALIZE_SB_CTRL_SUCCEEDED);
		break;

	case Channel::INITIALIZE_SB_CTRL_SUCCEEDED:

		switch (chan._sb_state) {
		case Superblock::INVALID: ASSERT_NEVER_REACHED;
		case Superblock::NORMAL:

			_chan_idx_queue.dequeue(chan_idx);
			chan._reset();
			progress = true;
			break;

		case Superblock::REKEYING: _resume_request(chan, chan_idx, progress, Request::REKEY); break;
		case Superblock::EXTENDING_VBD: _resume_request(chan, chan_idx, progress, Request::EXTEND_VBD); break;
		case Superblock::EXTENDING_FT: _resume_request(chan, chan_idx, progress, Request::EXTEND_FT); break;
		}
		break;

	default: break;
	}
}


void Request_pool::_forward_to_sb_ctrl(Channel &chan, Channel_index chan_idx, bool &progress, Superblock_control_request::Type type)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:
		_gen_superblock_control_req(chan, chan_idx, progress, type, 0, Channel::FORWARD_TO_SB_CTRL_SUCCEEDED);
		break;
	case Channel::FORWARD_TO_SB_CTRL_SUCCEEDED:
		_mark_req_successful(chan, chan_idx, progress);
		break;
	default: break;
	}
}


void Request_pool::execute(bool &progress)
{
	if (_chan_idx_queue.empty())
		return;

	Channel_index const chan_idx { _chan_idx_queue.head() };
	ASSERT(chan_idx < NUM_CHANNELS);
	Channel &chan { _channels[chan_idx] };
	switch (chan._req._op) {
	case Request::READ: _execute_access_vbas(chan, chan_idx, progress, Superblock_control_request::READ_VBA); break;
	case Request::WRITE: _execute_access_vbas(chan, chan_idx, progress, Superblock_control_request::WRITE_VBA); break;
	case Request::SYNC: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::SYNC); break;
	case Request::REKEY: _execute_rekey(chan, chan_idx, progress); break;
	case Request::EXTEND_VBD: _execute_extend_tree(chan, chan_idx, Superblock_control_request::VBD_EXTENSION_STEP, progress); break;
	case Request::EXTEND_FT: _execute_extend_tree(chan, chan_idx, Superblock_control_request::FT_EXTENSION_STEP, progress); break;
	case Request::INITIALIZE: _execute_initialize(chan, chan_idx, progress); break;
	case Request::DEINITIALIZE: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::DEINITIALIZE); break;
	case Request::CREATE_SNAPSHOT: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::CREATE_SNAPSHOT); break;
	case Request::DISCARD_SNAPSHOT: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::DISCARD_SNAPSHOT); break;
	default: break;
	}
}


void Request_pool::submit_request(Module_request &mod_req)
{
	for (Channel_index chan_idx { 0 }; chan_idx < NUM_CHANNELS; chan_idx++) {
		if (_channels[chan_idx]._state == Channel::INVALID) {
			Request &req { *static_cast<Request *>(&mod_req) };
			switch (req._op) {
			case Request::INITIALIZE: ASSERT_NEVER_REACHED;
			case Request::SYNC:
			case Request::READ:
			case Request::WRITE:
			case Request::DEINITIALIZE:
			case Request::REKEY:
			case Request::EXTEND_VBD:
			case Request::EXTEND_FT:
			case Request::CREATE_SNAPSHOT:
			case Request::DISCARD_SNAPSHOT:

				mod_req.dst_request_id(chan_idx);
				_channels[chan_idx]._state = Channel::REQ_SUBMITTED;
				_channels[chan_idx]._req = req;
				_chan_idx_queue.enqueue((Channel_index)chan_idx);
				return;

			default: ASSERT_NEVER_REACHED;
			}
		}
	}
	ASSERT_NEVER_REACHED;
}


Request_pool::Request_pool()
{
	register_channels(_channels, NUM_CHANNELS);
	Channel_index const chan_idx { 0 };
	_chan_idx_queue.enqueue(chan_idx);
	_channels[chan_idx]._state = Channel::REQ_SUBMITTED;
	_channels[chan_idx]._req = Request {
		Request::INITIALIZE, false, 0, 0, 0, 0, 0, 0,
		INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };
}


bool Request_pool::_peek_completed_request(uint8_t *buf_ptr, size_t buf_size)
{
	for (Channel &chan : _channels) {
		if (chan._req._op != Request::INVALID && chan._state == Channel::REQ_COMPLETE) {
			ASSERT(sizeof(chan._req) <= buf_size);
			memcpy(buf_ptr, &chan._req, sizeof(chan._req));
			return true;
		}
	}
	return false;
}


void Request_pool::_drop_completed_request(Module_request &req)
{
	Channel_index chan_idx { 0 };
	chan_idx = req.dst_request_id();
	ASSERT(chan_idx < NUM_CHANNELS);
	Channel &chan { _channels[chan_idx] };
	ASSERT(chan._req._op != Request::INVALID && chan._state == Channel::REQ_COMPLETE);
	chan._reset();
}


void Request_pool_channel::_generated_req_complete(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("request_pool: request (", _req, ") failed because generated request failed)");
		_req._success = false;
		_state = REQ_COMPLETE;
	} else
		_state = (State)state_uint;
}


void Request_pool_channel::_reset()
{
	_req = Request { };
	_state = INVALID;
	_sb_state = Superblock::INVALID;
	_num_blks = _vba = _num_requests_preponed = 0;
	_request_finished = _generated_req_success = false;
}
