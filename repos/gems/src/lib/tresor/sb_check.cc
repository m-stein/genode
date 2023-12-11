/*
 * \brief  Module for checking all hashes of a superblock and its hash trees
 * \author Martin Stein
 * \date   2023-05-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/sb_check.h>
#include <tresor/vbd_check.h>
#include <tresor/ft_check.h>
#include <tresor/block_io.h>
#include <tresor/trust_anchor.h>
#include <tresor/hash.h>

using namespace Tresor;

Sb_check_request::Sb_check_request(Module_id src_mod, Module_channel_id src_chan, bool &success)
:
	Module_request { src_mod, src_chan, SB_CHECK }, _success { success }
{ }


void Sb_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("sb check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


bool Sb_check_channel::_check_snap(bool &progress)
{
	Snapshot &snap { _sb.snapshots.items[_snap_idx] };
	if (snap.valid) {
		log(snap);
		if (snap.gen > _sb.last_secured_generation) {
			_mark_req_failed(progress, "snap generation newer than last secured generation");;
			return false;
		}
		Snapshot &snap { _sb.snapshots.items[_snap_idx] };
		_tree_root.construct(snap.pba, snap.gen, snap.hash, snap.max_level, _sb.degree, snap.nr_of_leaves);
		_generate_req<Vbd_check_request>(CHECK_SNAP_SUCCESSFUL, progress, *_tree_root);
		if (VERBOSE_CHECK)
			log("  check snap ", _snap_idx, " (", snap, ")");
	} else {
		_state = CHECK_SNAP_SUCCESSFUL;
		progress = true;
		if (VERBOSE_CHECK)
			log("  skip snap ", _snap_idx, " as it is unused");
	}
	return true;
}


void Sb_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_state) {
	case REQ_SUBMITTED:

		_sb_idx = 0;
		_generate_req<Trust_anchor::Read_hash>(READ_SB_HASH_SUCCEEDED, progress, _hash);
		break;

	case READ_SB_HASH_SUCCEEDED: _generate_req<Block_io::Read>(READ_BLK_SUCCESSFUL, progress, _sb_idx, _blk); break;
	case READ_BLK_SUCCESSFUL:

		if (check_hash(_blk, _hash)) {
			_sb.decode_from_blk(_blk);
			if (VERBOSE_CHECK)
				log("check superblock ", _sb_idx, " hash ", _hash, "\n  read superblock");

			if (!_sb.valid()) {
				_mark_req_failed(progress, "superblock marked invalid");;
				break;
			}
			_snap_idx = 0;
			if (_check_snap(progress))
				break;
		} else
			if (_sb_idx < MAX_SUPERBLOCK_INDEX) {
				_sb_idx++;
				_generate_req<Block_io::Read>(READ_BLK_SUCCESSFUL, progress, _sb_idx, _blk);
			} else
				_mark_req_failed(progress, "superblock not found");
		break;

	case CHECK_SNAP_SUCCESSFUL:

		if (_snap_idx < MAX_SNAP_IDX) {
			_snap_idx++;
			if (_check_snap(progress))
				break;
		} else {
			_tree_root.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree, _sb.free_leaves);
			_generate_req<Ft_check_request>(CHECK_FT_SUCCESSFUL, progress, *_tree_root);
			if (VERBOSE_CHECK)
				log("  check free tree");
		}
		break;

	case CHECK_FT_SUCCESSFUL:

		_tree_root.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves);
		_generate_req<Ft_check_request>(CHECK_MT_SUCCESSFUL, progress, *_tree_root);
		if (VERBOSE_CHECK)
			log("  check meta tree");
		break;

	case CHECK_MT_SUCCESSFUL: _mark_req_successful(progress); break;
	default: break;
	}
}


void Sb_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("sb check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Sb_check_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Sb_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Sb_check::Sb_check()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Sb_check::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
