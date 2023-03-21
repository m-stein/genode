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

using namespace Genode;
using namespace Cbe;


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
			channel._request = Cbe::Request(channel._request.operation(),
			                                true /* success */,
			                                channel._request.block_number(),
			                                channel._gen /* offset */,
			                                channel._request.count(),
			                                channel._request.key_id(),
			                                channel._request.tag(),
			                                channel._request.src_module_id(),
			                                channel._request.src_request_id());
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

			channel._request = Cbe::Request(Request::Operation::REKEY,
			                                false, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);
			indices.enqueue(idx);
			progress = true;

			break;
		case Superblock_state::EXTENDING_VBD:

			channel._state = Channel::State::SUBMITTED;

			channel._request = Cbe::Request(Request::Operation::EXTEND_VBD,
			                                false, 0, 0, 0, 0, 0,
			                                INVALID_MODULE_ID,
			                                INVALID_MODULE_REQUEST_ID);

			indices.enqueue(idx);

			progress = true;

			break;
		case Superblock_state::EXTENDING_FT:

			channel._state = Channel::State::SUBMITTED;

			channel._request = Cbe::Request(Request::Operation::EXTEND_FT,
			                                false, 0, 0, 0, 0, 0,
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
/*
		Execute_Rekey (Obj.Jobs, Obj.Indices, Idx, Progress);
*/
		throw Not_implemented { };

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

			Request &req { *dynamic_cast<Request *>(&mod_req) };
			req.dst_request_id(idx);
			switch (req.operation()) {
			case Request::INITIALIZE:

				class Exception_1 { };
				throw Exception_1 { };

			case Request::SYNC:
			case Request::READ:
			case Request::WRITE:
			case Request::DEINITIALIZE:

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
