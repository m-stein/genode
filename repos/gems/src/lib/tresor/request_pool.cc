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

Request::Request(Module_id src_module_id, Module_channel_id src_chan_id, Operation op, Virtual_block_address vba,
                 Request_offset offset, Number_of_blocks count, Key_id key_id, Request_tag tag, Generation &gen, bool &success)
:
	Module_request(src_module_id, src_chan_id, REQUEST_POOL), _op(op), _vba(vba), _offset(offset),
	_count(count), _key_id(key_id), _tag(tag), _gen(gen), _success(success)
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


void Request_pool_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_chan_queue.dequeue(*this);
	_req_ptr = nullptr;
	progress = true;
}


void Request_pool_channel::_try_prepone_requests(bool &progress)
{
	enum { MAX_NUM_REQUESTS_PREPONED = 8 };
	bool requests_preponed { false };
	bool at_req_that_cannot_be_preponed { false };
	_num_requests_preponed = 0;

	while (_num_requests_preponed < MAX_NUM_REQUESTS_PREPONED &&
	       !at_req_that_cannot_be_preponed &&
	       !_chan_queue.is_tail(*this)) {

		switch (_chan_queue.next(*this)._req_ptr->_op) {
		case Request::READ:
		case Request::WRITE:
		case Request::SYNC:
		case Request::DISCARD_SNAPSHOT:

			_chan_queue.move_one_slot_towards_tail(*this);
			_num_requests_preponed++;
			requests_preponed = true;
			progress = true;
			break;

		default:

			at_req_that_cannot_be_preponed = true;
			break;
		}
	}
	if (!requests_preponed) {
		_state = PREPONED_REQUESTS_COMPLETE;
		progress = true;
	}
}


void Request_pool_channel::_resume_request(bool &progress, Request::Operation op)
{
	_state = REQ_SUBMITTED;
	_req_ptr->_op = op;
	progress = true;
}


void Request_pool::execute(bool &progress)
{
	if (!_chan_queue.empty())
		_chan_queue.head().execute(_sb_control, _trust_anchor, _vbd, _client_data, _block_io, _free_tree, _meta_tree, _crypto, progress);
}


void Request_pool_channel::execute(Superblock_control &sb_control, Trust_anchor &trust_anchor, Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree, Crypto &crypto, bool &progress)
{
	Request &req { *_req_ptr };
	switch (_req_ptr->_op) {
	case Request::READ:

		switch (_state) {
		case REQ_SUBMITTED:
			_read_vba.generate(*this, READ_VBA, READ_VBA_SUCCEEDED, progress, _req_ptr->_vba + _num_blks, _req_ptr->_offset, _req_ptr->_tag);
			break;
		case READ_VBA: progress |= _read_vba.execute(sb_control, vbd, client_data, block_io, crypto); break;
		case READ_VBA_SUCCEEDED:
			if (++_num_blks < _req_ptr->_count)
				_read_vba.generate(*this, READ_VBA, READ_VBA_SUCCEEDED, progress, _req_ptr->_vba + _num_blks, _req_ptr->_offset, _req_ptr->_tag);
			else
				_mark_req_successful(progress);
			break;
		default: break;
		}
		break;

	case Request::WRITE:

		switch (_state) {
		case REQ_SUBMITTED:
			_write_vba.generate(*this, WRITE_VBA, WRITE_VBA_SUCCEEDED, progress, _req_ptr->_vba + _num_blks, _req_ptr->_offset, _req_ptr->_tag);
			break;
		case WRITE_VBA: progress |= _write_vba.execute(sb_control, vbd, client_data, block_io, free_tree, meta_tree, crypto); break;
		case WRITE_VBA_SUCCEEDED:
			if (++_num_blks < _req_ptr->_count)
				_write_vba.generate(*this, WRITE_VBA, WRITE_VBA_SUCCEEDED, progress, _req_ptr->_vba + _num_blks, _req_ptr->_offset, _req_ptr->_tag);
			else
				_mark_req_successful(progress);
			break;
		default: break;
		}
		break;

	case Request::SYNC:

		switch(_state) {
		case REQ_SUBMITTED: _sync_sb_control.generate(*this, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress); break;
		case SB_CONTROL_REQ: progress |= _sync_sb_control.execute(sb_control, block_io, trust_anchor); break;
		case SB_CONTROL_REQ_SUCCEEDED: _mark_req_successful(progress); break;
		default: break;
		}
		break;

	case Request::REKEY:

		switch (_state) {
		case REQ_SUBMITTED: _rekey.generate(*this, STATE_REKEY, STATE_REKEY_SUCCEEDED, progress, _request_finished); break;
		case STATE_REKEY: progress |= _rekey.execute(sb_control, vbd, free_tree, meta_tree, block_io, crypto, trust_anchor); break;
		case STATE_REKEY_SUCCEEDED:

			if (_request_finished)
				_mark_req_successful(progress);
			else
				_try_prepone_requests(progress);
			break;

		case PREPONED_REQUESTS_COMPLETE: _rekey.generate(*this, STATE_REKEY, STATE_REKEY_SUCCEEDED, progress, _request_finished); break;
		default: break;
		}
		break;

	case Request::EXTEND_VBD:

		switch (_state) {
		case REQ_SUBMITTED: _extend_vbd.generate(*this, EXTEND_VBD, EXTEND_VBD_SUCCEEDED, progress, _req_ptr->_count, _request_finished); break;
		case EXTEND_VBD: progress |= _extend_vbd.execute(sb_control, vbd, free_tree, meta_tree, block_io, trust_anchor); break;
		case EXTEND_VBD_SUCCEEDED:

			if (_request_finished)
				_mark_req_successful(progress);
			else
				_try_prepone_requests(progress);
			break;

		case PREPONED_REQUESTS_COMPLETE: _extend_vbd.generate(*this, EXTEND_VBD, EXTEND_VBD_SUCCEEDED, progress, _req_ptr->_count, _request_finished); break;
		default: break;
		}
		break;

	case Request::EXTEND_FT:

		switch (_state) {
		case REQ_SUBMITTED: _extend_ft.generate(*this, EXTEND_FT, EXTEND_FT_SUCCEEDED, progress, _req_ptr->_count, _request_finished); break;
		case EXTEND_FT: progress |= _extend_ft.execute(sb_control, free_tree, meta_tree, block_io, trust_anchor); break;
		case EXTEND_FT_SUCCEEDED:

			if (_request_finished)
				_mark_req_successful(progress);
			else
				_try_prepone_requests(progress);
			break;

		case PREPONED_REQUESTS_COMPLETE: _extend_ft.generate(*this, EXTEND_FT, EXTEND_FT_SUCCEEDED, progress, _req_ptr->_count, _request_finished); break;
		default: break;
		}
		break;

	case Request::INITIALIZE:

		switch (_state) {
		case REQ_SUBMITTED: _init_sb_control.generate(*this, INIT_SB_CONTROL, INIT_SB_CONTROL_SUCCEEDED, progress, _sb_state); break;
		case INIT_SB_CONTROL: progress |= _init_sb_control.execute(sb_control, block_io, crypto, trust_anchor); break;
		case INIT_SB_CONTROL_SUCCEEDED:

			switch (_sb_state) {
			case Superblock::INVALID: ASSERT_NEVER_REACHED;
			case Superblock::NORMAL:

				_chan_queue.dequeue(*this);
				_reset();
				progress = true;
				break;

			case Superblock::REKEYING: _resume_request(progress, Request::REKEY); break;
			case Superblock::EXTENDING_VBD: _resume_request(progress, Request::EXTEND_VBD); break;
			case Superblock::EXTENDING_FT: _resume_request(progress, Request::EXTEND_FT); break;
			}
			break;

		default: break;
		}
		break;

	case Request::DEINITIALIZE:

		switch(_state) {
		case REQ_SUBMITTED: _deinit_sb_control.generate(*this, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress); break;
		case SB_CONTROL_REQ: progress |= _deinit_sb_control.execute(sb_control, block_io, crypto, trust_anchor); break;
		case SB_CONTROL_REQ_SUCCEEDED: _mark_req_successful(progress); break;
		default: break;
		}
		break;

	case Request::CREATE_SNAPSHOT:

		switch(_state) {
		case REQ_SUBMITTED: _create_snap.generate(*this, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, req._gen); break;
		case SB_CONTROL_REQ: progress |= _create_snap.execute(sb_control, block_io, trust_anchor); break;
		case SB_CONTROL_REQ_SUCCEEDED: _mark_req_successful(progress); break;
		default: break;
		}
		break;

	case Request::DISCARD_SNAPSHOT:

		switch(_state) {
		case REQ_SUBMITTED: _discard_snap.generate(*this, SB_CONTROL_REQ, SB_CONTROL_REQ_SUCCEEDED, progress, req._gen); break;
		case SB_CONTROL_REQ: progress |= _discard_snap.execute(sb_control, block_io, trust_anchor); break;
		case SB_CONTROL_REQ_SUCCEEDED: _mark_req_successful(progress); break;
		default: break;
		}
		break;

	default: break;
	}
}


Request_pool::Request_pool(Superblock_control &sb_control, Trust_anchor &trust_anchor, Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree, Crypto &crypto)
:
	_sb_control(sb_control), _trust_anchor(trust_anchor), _vbd(vbd), _client_data(client_data), _block_io(block_io),
	_free_tree(free_tree), _meta_tree(meta_tree), _crypto(crypto)
{
	for (Module_channel_id id { 0 }; id < NUM_CHANNELS; id++) {
		_channels[id].construct(id, _chan_queue);
		add_channel(*_channels[id]);
	}
	ASSERT(try_submit_request(_init_req));
}


void Request_pool_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("request_pool: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_chan_queue.dequeue(*this);
		_req_ptr = nullptr;
	} else
		_state = (State)state_uint;
}


void Request_pool_channel::generated_req_failed(bool &progress)
{
	error("request_pool: request (", *_req_ptr, ") failed because generated request failed)");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_chan_queue.dequeue(*this);
	_req_ptr = nullptr;
	progress = true;
}


void Request_pool_channel::_reset()
{
	_state = INVALID;
	_sb_state = Superblock::INVALID;
	_num_blks = _num_requests_preponed = 0;
	_request_finished = false;
}


Request_pool_channel &Request_pool_channel_queue::head() const
{
	ASSERT(!empty());
	return *_slots[_head];
}


void Request_pool_channel_queue::enqueue(Channel &chan)
{
	ASSERT(!full());
	_slots[_tail] = &chan;
	_tail = (_tail + 1) % NUM_SLOTS;
	_num_used_slots++;
}


void Request_pool_channel_queue::move_one_slot_towards_tail(Channel const &chan)
{
	Slot_index slot_idx { _head };
	Slot_index next_slot_idx;
	ASSERT(!empty());
	while (1) {
		if (slot_idx < NUM_SLOTS - 1)
			next_slot_idx = slot_idx + 1;
		else
			next_slot_idx = 0;

		ASSERT(next_slot_idx != _tail);
		if (_slots[slot_idx] == &chan) {
			Channel *chan_ptr = _slots[next_slot_idx];
			_slots[next_slot_idx] = _slots[slot_idx];
			_slots[slot_idx] = chan_ptr;
			return;
		}
		slot_idx = next_slot_idx;
	}
}


bool Request_pool_channel_queue::is_tail(Channel const &chan) const
{
	Slot_index slot_idx;
	ASSERT(!empty());
	if (_tail)
		slot_idx = _tail - 1;
	else
		slot_idx = NUM_SLOTS - 1;

	return _slots[slot_idx] == &chan;
}


Request_pool_channel &Request_pool_channel_queue::next(Channel const &chan) const
{
	Slot_index slot_idx { _head };
	Slot_index next_slot_idx;
	ASSERT(!empty());
	while (1) {
		if (slot_idx < NUM_SLOTS - 1)
			next_slot_idx = slot_idx + 1;
		else
			next_slot_idx = 0;

		ASSERT(next_slot_idx != _tail);
		if (_slots[slot_idx] == &chan)
			return *_slots[next_slot_idx];
		else
			slot_idx = next_slot_idx;
	}
}


void Request_pool_channel_queue::dequeue(Channel const &chan)
{
	ASSERT(!empty() && &head() == &chan);
	_head = (_head + 1) % NUM_SLOTS;
	_num_used_slots--;
}


void Request_pool_channel::_request_submitted(Module_request &req)
{
	_reset();
	_req_ptr = static_cast<Request *>(&req);
	_state = REQ_SUBMITTED;
	_chan_queue.enqueue(*this);
}
