/*
 * \brief  Module for request pool
 * \author Martin Stein
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* cbe tester includes */
#include <request_pool.h>
#include <superblock_control.h>

using namespace Genode;
using namespace Cbe;


char const *Request::op_to_string(Operation op) { return to_string(op); }


void Request_pool::_execute_read(Channel &channel, Index_queue &indices,
                                 Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
		channel._nr_of_blks = 0;

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_READ_VBA,
			.pl_idx = idx,
			.blk_nr = channel._request.block_number() + channel._nr_of_blks,
			.idx    = 0
		};

		channel._state = Channel::State::READ_VBA_AT_SB_CTRL_PENDING;
		progress       = true;

		break;
	case Channel::State::READ_VBA_AT_SB_CTRL_COMPLETE:
		if (channel._prim.succ) {

			channel._nr_of_blks += 1;

			if (channel._nr_of_blks < channel._request.count()) {

				channel._prim = {
					.op     = Channel::Generated_prim::Type::READ,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_READ_VBA,
					.pl_idx = idx,
					.blk_nr = channel._request.block_number() + channel._nr_of_blks,
					.idx    = 0
				};

				channel._state = Channel::State::READ_VBA_AT_SB_CTRL_PENDING;
				progress = true;
			} else {
				channel._request.success(true);
				channel._state = Channel::State::COMPLETE;
				indices.dequeue(idx);
				progress = true;
			}
		} else {
			channel._request.success(false);
			channel._state = Channel::State::COMPLETE;
			indices.dequeue(idx);
			progress = true;
		}
		break;
	default:
		break;
	}
}


void Request_pool::_execute_write(Channel &channel, Index_queue &indices,
                                  Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._nr_of_blks = 0;

		channel._prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_WRITE_VBA,
			.pl_idx = idx,
			.blk_nr = channel._request.block_number() + channel._nr_of_blks,
			.idx    = 0
		};
		channel._state = Channel::State::WRITE_VBA_AT_SB_CTRL_PENDING;
		progress = true;
		break;

	case Channel::State::WRITE_VBA_AT_SB_CTRL_COMPLETE:

		if (channel._prim.succ) {
			channel._nr_of_blks += 1;

			if (channel._nr_of_blks < channel._request.count()) {

				channel._prim = {
					.op     = Channel::Generated_prim::Type::WRITE,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_WRITE_VBA,
					.pl_idx = idx,
					.blk_nr = channel._request.block_number() + channel._nr_of_blks,
					.idx    = 0
				};

				channel._state = Channel::State::WRITE_VBA_AT_SB_CTRL_PENDING;
				progress       = true;

			} else {

				channel._request.success(true);
				channel._state = Channel::State::COMPLETE;
				indices.dequeue(idx);
				progress = true;

			}
		} else {

			channel._request.success(false);
			channel._state = Channel::State::COMPLETE;
			indices.dequeue(idx);
			progress = true;

		}

		break;
	default:
		break;
	}
}


void Request_pool::_execute_sync(Channel &channel, Index_queue &indices,
                                 Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_SYNC,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};

		channel._state = Channel::State::SYNC_AT_SB_CTRL_PENDING;
		progress       = true;

		break;
	case Channel::State::SYNC_AT_SB_CTRL_COMPLETE:

		if (channel._prim.succ) {
			channel._request.success(true);
			channel._request.offset(channel._gen);
		} else
			channel._request.success(false);

		channel._state = Channel::State::COMPLETE;
		indices.dequeue(idx);
		progress = true;

		break;
	default:
		break;
	}
}


void Request_pool::_execute_rekey(Channel     &chan,
                                  Index_queue &indices,
                                  Slots_index  idx,
                                  bool        &progress)
{
	Request &req { chan._request };

	switch (chan._state) {
	case Channel::State::SUBMITTED:

		chan._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_INIT_REKEY,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		chan._state = Channel::State::REKEY_INIT_PENDING;
		progress    = true;
		break;

	case Channel::State::SUBMITTED_RESUME_REKEYING:

		chan._prim = { };
		chan._nr_of_requests_preponed = 0;
		chan._state = Channel::State::PREPONE_REQUESTS_PENDING;
		progress = true;
		break;

	case Channel::State::REKEY_INIT_COMPLETE:

		if (!chan._prim.succ) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		chan._nr_of_requests_preponed = 0;
		chan._state = Channel::State::PREPONE_REQUESTS_PENDING;
		progress = true;
		break;

	case Channel::State::REKEY_VBA_COMPLETE:

		if (!chan._prim.succ) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		if (chan._request_finished) {

			req.success(true);
			chan._state = Channel::State::COMPLETE;
			_indices.dequeue(idx);
			progress = true;
			break;

		} else {

			chan._nr_of_requests_preponed = 0;
			chan._state = Channel::State::PREPONE_REQUESTS_PENDING;
			progress = true;
			break;
		}

	case Channel::State::PREPONE_REQUESTS_PENDING:
	{
		bool requests_preponed { false };
		while (true) {

			bool exit_loop { false };

			if (chan._nr_of_requests_preponed >= MAX_NR_OF_REQUESTS_PREPONED_AT_A_TIME ||
			    indices.item_is_tail(idx))
				break;

			Pool_index const next_idx { indices.next_item(idx) };
			switch (_channels[next_idx]._request.operation()) {
			case Request::READ:
			case Request::WRITE:
			case Request::SYNC:
			case Request::DISCARD_SNAPSHOT:

				indices.move_one_item_towards_tail(idx);
				chan._nr_of_requests_preponed++;
				requests_preponed = true;
				progress = true;
				break;

			default:

				exit_loop = true;
				break;
			}
			if (exit_loop)
				break;
		}
		if (!requests_preponed) {
			chan._state = Channel::State::PREPONE_REQUESTS_COMPLETE;
			progress = true;
		}
		break;
	}
	case Channel::State::PREPONE_REQUESTS_COMPLETE:

		chan._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_REKEY_VBA,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};
		chan._state = Channel::State::REKEY_VBA_PENDING;
		progress    = true;
		break;

	default:

		break;
	}
}


void Request_pool::_execute_initialize(Channel &channel, Index_queue &indices,
                                       Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_INITIALIZE,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};

		channel._state = Channel::State::INITIALIZE_SB_CTRL_PENDING;
		progress       = true;

		break;

	case Channel::State::INITIALIZE_SB_CTRL_COMPLETE:

		if (not channel._prim.succ) {
			class Initialize_primitive_not_successfull { };
			throw Initialize_primitive_not_successfull { };
		}

		switch (channel._sb_state) {
		case Superblock_state::INVALID:
			class Initialize_sb_ctrl_invalid { };
			throw Initialize_sb_ctrl_invalid { };

			break;
		case Superblock_state::NORMAL:

			indices.dequeue(idx);
			channel.invalidate();
			progress = true;

			break;

		case Superblock_state::REKEYING:

			class Exception_rekeying { };
			throw Exception_rekeying { };
			channel._request = Cbe::Request(Request::Operation::REKEY,
			                                false, 0, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);
			indices.enqueue(idx);
			progress = true;

			break;
		case Superblock_state::EXTENDING_VBD:

			class Exception_ext_vbd { };
			throw Exception_ext_vbd { };
			channel._state = Channel::State::SUBMITTED;

			channel._request = Cbe::Request(Request::Operation::EXTEND_VBD,
			                                false, 0, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);

			indices.enqueue(idx);

			progress = true;

			break;
		case Superblock_state::EXTENDING_FT:

			class Exception_ext_ft { };
			throw Exception_ext_ft { };
			channel._state = Channel::State::SUBMITTED;

			channel._request = Cbe::Request(Request::Operation::EXTEND_FT,
			                                false, 0, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);

			indices.enqueue(idx);

			progress = true;

			break;
		}

		break;
	default:
		break;
	}
}


void Request_pool::_execute_deinitialize(Channel &channel, Index_queue &indices,
                                         Slots_index const idx, bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_POOL_SB_CTRL_DEINITIALIZE,
			.pl_idx = idx,
			.blk_nr = 0,
			.idx    = 0
		};

		channel._state = Channel::State::DEINITIALIZE_SB_CTRL_PENDING;
		progress       = true;

		break;

	case Channel::State::DEINITIALIZE_SB_CTRL_COMPLETE:

		if (not channel._prim.succ) {
			class Deinitialize_primitive_not_successfull { };
			throw Deinitialize_primitive_not_successfull { };
		}

		channel._request.success(true);
		channel._state = Channel::State::COMPLETE;
		indices.dequeue(idx);
		progress = true;

		break;
	default:
		break;
	}
}


void Request_pool::execute(bool &progress)
{
	if (_indices.empty())
		return;

	class Not_implemented { };

	auto const idx = _indices.head();

	/* XXX idx array check ? */

	Channel &channel = _channels[idx];
	Request &request = { channel._request };

	switch (request.operation()) {
	case Cbe::Request::Operation::READ:
		_execute_read(channel, _indices, idx, progress);
		break;
	case Cbe::Request::Operation::WRITE:
		_execute_write(channel, _indices, idx, progress);
		break;
	case Cbe::Request::Operation::SYNC:
		_execute_sync(channel, _indices, idx, progress);
		break;
	case Cbe::Request::Operation::REKEY:
		_execute_rekey(channel, _indices, idx, progress);
		break;
	case Cbe::Request::Operation::EXTEND_VBD:
/*
               Execute_Extend_VBD (Obj.Jobs, Obj.Indices, Idx, Progress);
*/
		throw Not_implemented { };

		break;
	case Cbe::Request::Operation::EXTEND_FT:
/*
               Execute_Extend_FT (Obj.Jobs, Obj.Indices, Idx, Progress);
*/
		throw Not_implemented { };

		break;
	case Cbe::Request::Operation::CREATE_SNAPSHOT:
/*
               Execute_Create_Snapshot (Obj.Jobs, Obj.Indices, Idx, Progress);
*/
		throw Not_implemented { };

		break;
	case Cbe::Request::Operation::DISCARD_SNAPSHOT:
/*
               Execute_Discard_Snapshot (Obj.Jobs, Obj.Indices, Idx, Progress);
*/
		throw Not_implemented { };

		break;
	case Cbe::Request::Operation::INITIALIZE:
		_execute_initialize(channel, _indices, idx, progress);
		break;
	case Cbe::Request::Operation::DEINITIALIZE:
		_execute_deinitialize(channel, _indices, idx, progress);
		break;
	default:
		break;
	}
}


void Request_pool::submit_request(Module_request &mod_req)
{
	for (unsigned long idx { 0 }; idx < NR_OF_CHANNELS; idx++) {
		if (_channels[idx]._state == Channel::INVALID) {
			Request &req { *static_cast<Request *>(&mod_req) };
			switch (req.operation()) {
			case Request::INITIALIZE:

				class Exception_1 { };
				throw Exception_1 { };

			case Request::SYNC:
			case Request::READ:
			case Request::WRITE:
			case Request::DEINITIALIZE:
			case Request::REKEY:

				mod_req.dst_request_id(idx);
				_channels[idx]._state = Channel::SUBMITTED;
				_channels[idx]._request = req;
				_indices.enqueue(idx);
				return;

			default:

				class Exception_2 { };
				throw Exception_2 { };
			}
		}
	}
	class Exception_3 { };
	throw Exception_3 { };
}


bool Request_pool::_peek_generated_request(uint8_t *buf_ptr,
                                           size_t   buf_size)
{
	if (_indices.empty())
		return false;

	Slots_index const idx { _indices.head() };
	Channel const &chan { _channels[idx] };
	Superblock_control_request::Type scr_type;

	switch (chan._state) {
	case Channel::READ_VBA_AT_SB_CTRL_PENDING:  scr_type = Superblock_control_request::READ_VBA; break;
	case Channel::WRITE_VBA_AT_SB_CTRL_PENDING: scr_type = Superblock_control_request::WRITE_VBA; break;
	case Channel::SYNC_AT_SB_CTRL_PENDING:      scr_type = Superblock_control_request::SYNC; break;
	case Channel::INITIALIZE_SB_CTRL_PENDING:   scr_type = Superblock_control_request::INITIALIZE; break;
	case Channel::DEINITIALIZE_SB_CTRL_PENDING: scr_type = Superblock_control_request::DEINITIALIZE; break;
	case Channel::REKEY_INIT_PENDING:           scr_type = Superblock_control_request::INITIALIZE_REKEYING; break;
	case Channel::REKEY_VBA_PENDING:            scr_type = Superblock_control_request::REKEY_VBA; break;
	default: return false;
	}
	Superblock_control_request::create(
		buf_ptr, buf_size, REQUEST_POOL, idx, scr_type, nullptr, 0,
		chan._request.offset(), chan._request.tag(),
		chan._prim.blk_nr);

	return true;
}


void Request_pool::_drop_generated_request(Module_request &mod_req)
{
	unsigned long const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._state) {
	case Channel::READ_VBA_AT_SB_CTRL_PENDING: chan._state = Channel::READ_VBA_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::WRITE_VBA_AT_SB_CTRL_PENDING: chan._state = Channel::WRITE_VBA_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::SYNC_AT_SB_CTRL_PENDING: chan._state = Channel::SYNC_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::REKEY_INIT_PENDING: chan._state = Channel::REKEY_INIT_IN_PROGRESS; break;
	case Channel::REKEY_VBA_PENDING: chan._state = Channel::REKEY_VBA_IN_PROGRESS; break;
	case Channel::VBD_EXTENSION_STEP_PENDING: chan._state = Channel::VBD_EXTENSION_STEP_IN_PROGRESS; break;
	case Channel::FT_EXTENSION_STEP_PENDING: chan._state = Channel::FT_EXTENSION_STEP_IN_PROGRESS; break;
	case Channel::CREATE_SNAP_AT_SB_CTRL_PENDING: chan._state = Channel::CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::DISCARD_SNAP_AT_SB_CTRL_PENDING: chan._state = Channel::DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS; break;
	case Channel::INITIALIZE_SB_CTRL_PENDING: chan._state = Channel::INITIALIZE_SB_CTRL_IN_PROGRESS; break;
	case Channel::DEINITIALIZE_SB_CTRL_PENDING: chan._state = Channel::DEINITIALIZE_SB_CTRL_IN_PROGRESS; break;
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


void Request_pool::generated_request_complete(Module_request &mod_req)
{
	unsigned long const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case SUPERBLOCK_CONTROL:
	{
		Superblock_control_request &gen_req { *static_cast<Superblock_control_request *>(&mod_req) };
		chan._prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::READ_VBA_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::READ_VBA_AT_SB_CTRL_COMPLETE; break;
		case Channel::WRITE_VBA_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::WRITE_VBA_AT_SB_CTRL_COMPLETE; break;
		case Channel::SYNC_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::SYNC_AT_SB_CTRL_COMPLETE; break;
		case Channel::VBD_EXTENSION_STEP_IN_PROGRESS: chan._state = Channel::VBD_EXTENSION_STEP_COMPLETE; break;
		case Channel::FT_EXTENSION_STEP_IN_PROGRESS: chan._state = Channel::FT_EXTENSION_STEP_COMPLETE; break;
		case Channel::CREATE_SNAP_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::CREATE_SNAP_AT_SB_CTRL_COMPLETE; break;
		case Channel::DISCARD_SNAP_AT_SB_CTRL_IN_PROGRESS: chan._state = Channel::DISCARD_SNAP_AT_SB_CTRL_COMPLETE; break;
		case Channel::INITIALIZE_SB_CTRL_IN_PROGRESS:
			chan._sb_state = gen_req.sb_state();
			chan._state = Channel::INITIALIZE_SB_CTRL_COMPLETE;
			break;
		case Channel::DEINITIALIZE_SB_CTRL_IN_PROGRESS: chan._state = Channel::DEINITIALIZE_SB_CTRL_COMPLETE; break;
		case Channel::REKEY_INIT_IN_PROGRESS: chan._state = Channel::REKEY_INIT_COMPLETE; break;
		case Channel::REKEY_VBA_IN_PROGRESS:
			chan._request_finished = gen_req.request_finished();
			chan._state = Channel::REKEY_VBA_COMPLETE;
			break;
		default:
			class Exception_2 { };
			throw Exception_2 { };
		}
		break;
	}
	default:
		class Exception_5 { };
		throw Exception_5 { };
	}
}


Request_pool::Request_pool()
{
	Slots_index const idx { 0 };
	_channels[idx]._state = Channel::SUBMITTED;
	_channels[idx]._request = Request {
		Request::INITIALIZE, false, 0, 0, 0, 0, 0, 0,
		INVALID_MODULE_ID, INVALID_MODULE_REQUEST_ID };

	_indices.enqueue(idx);
}


bool Request_pool::_peek_completed_request(uint8_t *buf_ptr,
                                           size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._request.operation() != Request::INVALID &&
		    channel._state == Channel::COMPLETE) {

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


void Request_pool::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	if (chan._request.operation() == Request::INVALID ||
	    chan._state != Channel::COMPLETE) {

		class Exception_2 { };
		throw Exception_2 { };
	}
	chan = Channel { };
}
