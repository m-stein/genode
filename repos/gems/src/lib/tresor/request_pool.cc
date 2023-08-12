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
	case Tresor::Request::INVALID: return "invalid";
	case Tresor::Request::READ: return "read";
	case Tresor::Request::WRITE: return "write";
	case Tresor::Request::SYNC: return "sync";
	case Tresor::Request::CREATE_SNAPSHOT: return "create_snapshot";
	case Tresor::Request::DISCARD_SNAPSHOT: return "discard_snapshot";
	case Tresor::Request::REKEY: return "rekey";
	case Tresor::Request::EXTEND_VBD: return "extend_vbd";
	case Tresor::Request::EXTEND_FT: return "extend_ft";
	case Tresor::Request::RESUME_REKEYING: return "resume_rekeying";
	case Tresor::Request::DEINITIALIZE: return "deinitialize";
	case Tresor::Request::INITIALIZE: return "initialize";
	}
	ASSERT_NEVER_REACHED;
}


void Request_pool::_gen_superblock_control_req(Channel &chan, Channel_index chan_idx, bool &progress,
                                               Superblock_control_request::Type type, Virtual_block_address vba,
                                               Channel::State complete_state)
{
	chan.generate_req<Superblock_control_request>(
		complete_state, progress, REQUEST_POOL, chan_idx, type, chan._req._offset, chan._req._tag,
		chan._req._count, vba, chan._generated_req_success, chan._request_finished,
		chan._sb_state, chan._req._gen);

	chan._state = Channel::GENERATED_REQUEST;
}


void Request_pool::_execute_access_vbas(Channel &chan, Channel_index chan_idx, bool &progress, Superblock_control_request::Type type)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		_gen_superblock_control_req(
			chan, chan_idx, progress, type, chan._req._vba + chan._nr_of_blks,
			Channel::ACCESS_VBA_AT_SB_CTRL_SUCCEEDED);
		break;

	case Channel::ACCESS_VBA_AT_SB_CTRL_SUCCEEDED:

		if (++chan._nr_of_blks < chan._req._count)
			_gen_superblock_control_req(
				chan, chan_idx, progress, type, chan._req._vba + chan._nr_of_blks,
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
	chan._state = Channel::COMPLETE;
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
	chan._state = Channel::COMPLETE;
	_chan_idx_queue.dequeue(chan_idx);
	progress = true;
}


void Request_pool::_try_prepone_requests(Channel &chan, Channel_index chan_idx, bool &progress)
{
	bool requests_preponed { false };
	bool at_req_that_cannot_be_preponed { false };
	chan._nr_of_requests_preponed = 0;
	while (chan._nr_of_requests_preponed < MAX_NR_OF_REQUESTS_PREPONED_AT_A_TIME &&
	       !at_req_that_cannot_be_preponed &&
	       !_chan_idx_queue.is_tail(chan_idx))
	{
		switch (_channels[_chan_idx_queue.next(chan_idx)]._req._op) {
		case Request::READ:
		case Request::WRITE:
		case Request::SYNC:
		case Request::DISCARD_SNAPSHOT:

			_chan_idx_queue.move_one_slot_towards_tail(chan_idx);
			chan._nr_of_requests_preponed++;
			requests_preponed = true;
			progress = true;
			break;

		default:

			at_req_that_cannot_be_preponed = true;
			break;
		}
	}
	if (!requests_preponed) {
		chan._state = Channel::PREPONE_REQUESTS_COMPLETE;
		progress = true;
	}
}


void Request_pool::_execute_extend_tree(Channel &chan, Channel_index chan_idx,
                                        Channel::State tree_ext_step_pending, bool &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		chan._vba = 0;
		chan._state = tree_ext_step_pending;
		progress = true;
		break;

	case Channel::TREE_EXTENSION_STEP_COMPLETE:

		if (_handle_failed_generated_req(chan, chan_idx, progress, __LINE__)) break;
		if (chan._request_finished)
			_mark_req_successful(chan, chan_idx, progress);
		else
			_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::PREPONE_REQUESTS_COMPLETE:

		chan._vba = 0;
		chan._state = tree_ext_step_pending;
		progress = true;
		break;

	default:

		break;
	}
}


void Request_pool::_execute_rekey(Channel &chan, Channel_index chan_idx, bool &progress)
{
	switch (chan._state) {
	case Channel::State::SUBMITTED:

		_gen_superblock_control_req(
			chan, chan_idx, progress, Superblock_control_request::INITIALIZE_REKEYING,
			0, Channel::REKEY_INIT_SUCCEEDED);
		break;

	case Channel::State::SUBMITTED_RESUME_REKEYING:

		_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::State::REKEY_INIT_SUCCEEDED:

		_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::State::REKEY_VBA_SUCCEEDED:

		if (chan._request_finished)
			_mark_req_successful(chan, chan_idx, progress);
		else
			_try_prepone_requests(chan, chan_idx, progress);
		break;

	case Channel::State::PREPONE_REQUESTS_COMPLETE:

		_gen_superblock_control_req(
			chan, chan_idx, progress, Superblock_control_request::REKEY_VBA,
			0, Channel::REKEY_VBA_SUCCEEDED);
		break;

	default:

		break;
	}
}


void Request_pool::_execute_initialize(Channel &chan, Channel_index chan_idx, bool &progress)
{
	switch (chan._state) {
	case Channel::State::SUBMITTED:

		chan._vba = 0;
		_gen_superblock_control_req(
			chan, chan_idx, progress, Superblock_control_request::INITIALIZE,
			0, Channel::INITIALIZE_SB_CTRL_SUCCEEDED);
		break;

	case Channel::State::INITIALIZE_SB_CTRL_SUCCEEDED:

		switch (chan._sb_state) {
		case Superblock::INVALID: ASSERT_NEVER_REACHED;
		case Superblock::NORMAL:

			_chan_idx_queue.dequeue(chan_idx);
			chan._req = { };
			chan._state = { Channel::INVALID };
			chan._nr_of_blks =  0;
			chan._sb_state = { Superblock::INVALID };
			progress = true;
			break;

		case Superblock::REKEYING:

			chan._req = Tresor::Request {
				Request::Operation::REKEY, false, 0, 0, 0, 0, 0, 0,
				INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };

			_chan_idx_queue.enqueue(chan_idx);
			progress = true;
			break;

		case Superblock::EXTENDING_VBD:

			chan._state = Channel::State::SUBMITTED;
			chan._req = Tresor::Request {
				Request::Operation::EXTEND_VBD, false, 0, 0, 0, 0, 0, 0,
				INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };

			_chan_idx_queue.enqueue(chan_idx);
			progress = true;
			break;

		case Superblock::EXTENDING_FT:

			chan._state = Channel::State::SUBMITTED;
			chan._req = Tresor::Request {
				Request::Operation::EXTEND_FT, false, 0, 0, 0, 0, 0, 0,
				INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };

			_chan_idx_queue.enqueue(chan_idx);
			progress = true;
			break;
		}
		break;

	default: break;
	}
}


void Request_pool::_forward_to_sb_ctrl(Channel &chan, Channel_index chan_idx, bool &progress, Superblock_control_request::Type type)
{
	switch (chan._state) {
	case Channel::State::SUBMITTED:
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
	ASSERT(chan_idx < NR_OF_CHANNELS);
	Channel &chan { _channels[chan_idx] };
	switch (chan._req._op) {
	case Tresor::Request::Operation::READ: _execute_access_vbas(chan, chan_idx, progress, Superblock_control_request::READ_VBA); break;
	case Tresor::Request::Operation::WRITE: _execute_access_vbas(chan, chan_idx, progress, Superblock_control_request::WRITE_VBA); break;
	case Tresor::Request::Operation::SYNC: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::SYNC); break;
	case Tresor::Request::Operation::REKEY: _execute_rekey(chan, chan_idx, progress); break;
	case Tresor::Request::Operation::EXTEND_VBD: _execute_extend_tree(chan, chan_idx, Channel::VBD_EXTENSION_STEP_PENDING, progress); break;
	case Tresor::Request::Operation::EXTEND_FT: _execute_extend_tree(chan, chan_idx, Channel::FT_EXTENSION_STEP_PENDING, progress); break;
	case Tresor::Request::Operation::INITIALIZE: _execute_initialize(chan, chan_idx, progress); break;
	case Tresor::Request::Operation::DEINITIALIZE: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::DEINITIALIZE); break;
	case Tresor::Request::Operation::CREATE_SNAPSHOT: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::CREATE_SNAPSHOT); break;
	case Tresor::Request::Operation::DISCARD_SNAPSHOT: _forward_to_sb_ctrl(chan, chan_idx, progress, Superblock_control_request::DISCARD_SNAPSHOT); break;
	default: break;
	}
}


void Request_pool::submit_request(Module_request &mod_req)
{
	for (Channel_index chan_idx { 0 }; chan_idx < NR_OF_CHANNELS; chan_idx++) {
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
				_channels[chan_idx]._state = Channel::SUBMITTED;
				_channels[chan_idx]._req = req;
				_chan_idx_queue.enqueue((Channel_index)chan_idx);
				return;

			default: ASSERT_NEVER_REACHED;
			}
		}
	}
	ASSERT_NEVER_REACHED;
}


bool Request_pool::_peek_generated_request(uint8_t *buf_ptr, size_t buf_size)
{
	if (_chan_idx_queue.empty())
		return false;

	Channel_index const chan_idx { _chan_idx_queue.head() };
	Channel &chan { _channels[chan_idx] };
	Request &req { chan._req };
	Superblock_control_request::Type req_type;
	switch (chan._state) {
	case Channel::VBD_EXTENSION_STEP_PENDING: req_type = Superblock_control_request::VBD_EXTENSION_STEP; break;
	case Channel::FT_EXTENSION_STEP_PENDING: req_type = Superblock_control_request::FT_EXTENSION_STEP; break;
	default: return false;
	}
	Superblock_control_request::create(
		buf_ptr, buf_size, REQUEST_POOL, chan_idx, req_type, req._offset, req._tag, req._count, chan._vba,
		chan._generated_req_success, chan._request_finished, chan._sb_state, req._gen);

	return true;
}


void Request_pool::_drop_generated_request(Module_request &mod_req)
{
	Channel_index const chan_idx { mod_req.src_request_id() };
	ASSERT(chan_idx < NR_OF_CHANNELS);
	Channel &chan { _channels[chan_idx] };
	switch (chan._state) {
	case Channel::VBD_EXTENSION_STEP_PENDING: chan._state = Channel::TREE_EXTENSION_STEP_IN_PROGRESS; break;
	case Channel::FT_EXTENSION_STEP_PENDING: chan._state = Channel::TREE_EXTENSION_STEP_IN_PROGRESS; break;
	default: ASSERT_NEVER_REACHED;
	}
}


void Request_pool::generated_request_complete(Module_request &mod_req)
{
	Channel_index const chan_idx { mod_req.src_request_id() };
	ASSERT(chan_idx < NR_OF_CHANNELS);
	Channel &chan { _channels[chan_idx] };
	switch (mod_req.dst_module_id()) {
	case SUPERBLOCK_CONTROL:
	{
		Superblock_control_request &gen_req { *static_cast<Superblock_control_request *>(&mod_req) };
		chan._generated_req_success = gen_req.success();
		switch (chan._state) {
		case Channel::TREE_EXTENSION_STEP_IN_PROGRESS:
			chan._state = Channel::TREE_EXTENSION_STEP_COMPLETE;
			chan._request_finished = gen_req.request_finished();
			break;
		case Channel::GENERATED_REQUEST: break;
		default: ASSERT_NEVER_REACHED;
		}
		break;
	}
	default: ASSERT_NEVER_REACHED;
	}
}


Request_pool::Request_pool()
{
	register_channels(_channels, NR_OF_CHANNELS);
	Channel_index const chan_idx { 0 };
	_chan_idx_queue.enqueue(chan_idx);
	_channels[chan_idx]._state = Channel::SUBMITTED;
	_channels[chan_idx]._req = Request {
		Request::INITIALIZE, false, 0, 0, 0, 0, 0, 0,
		INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };
}


bool Request_pool::_peek_completed_request(uint8_t *buf_ptr, size_t buf_size)
{
	for (Channel &chan : _channels) {
		if (chan._req._op != Request::INVALID && chan._state == Channel::COMPLETE) {
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
	ASSERT(chan_idx < NR_OF_CHANNELS);
	Channel &chan { _channels[chan_idx] };
	ASSERT(chan._req._op != Request::INVALID && chan._state == Channel::COMPLETE);
	chan._reset();
}
