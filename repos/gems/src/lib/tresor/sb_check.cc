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

using namespace Tresor;

Sb_check_request::Sb_check_request(Module_id src_mod, Module_request_id src_chan, bool &success)
:
	Module_request { src_mod, src_chan, SB_CHECK }, _success { success }
{ }


void Sb_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_gen_prim_success) {
		error("sb check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_sb_slot_state = DONE;
		_req_ptr = nullptr;
		return;
	}
	_sb_slot_state = (Sb_slot_state)state_uint;
}


void Sb_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_state) {
	case INSPECT_SBS:

		switch (_sb_slot_state) {
		case INIT:

			_gen_prim_blk_nr = _sb_slot_idx;
			_generate_req<Block_io::Read>(READ_DONE, progress, _gen_prim_blk_nr, _encoded_blk);
			break;

		case READ_DONE:
		{
			_sb_slot.decode_from_blk(_encoded_blk);
			Snapshot &snap { _sb_slot.curr_snap() };
			if (_sb_slot.valid() &&
			    snap.gen > _highest_gen) {

				_highest_gen = snap.gen;
				_last_sb_slot_idx = _sb_slot_idx;
			}
			if (_sb_slot_idx < MAX_SUPERBLOCK_INDEX) {

				_sb_slot_idx++;
				_sb_slot_state = INIT;
				progress = true;

			} else {

				_state = CHECK_SB;
				_sb_slot_idx = _last_sb_slot_idx;
				_sb_slot_state = INIT;
				progress = true;

				if (VERBOSE_CHECK)
					log("check superblock ", _sb_slot_idx);
			}
			break;
		}
		default:

			break;
		}
		break;

	case CHECK_SB:

		switch (_sb_slot_state) {
		case INIT:

			_gen_prim_blk_nr = _sb_slot_idx;
			_generate_req<Block_io::Read>(READ_DONE, progress, _gen_prim_blk_nr, _encoded_blk);
			if (VERBOSE_CHECK)
				log("  read superblock");
			break;

		case READ_DONE:

			_sb_slot.decode_from_blk(_encoded_blk);
			if (_sb_slot.valid()) {
				Snapshot &snap { _sb_slot.snapshots.items[_snap_idx] };
				if (snap.valid) {
					Snapshot &snap { _sb_slot.snapshots.items[_snap_idx] };
					_gen_prim_blk_nr = snap.pba;
					_generate_req<Vbd_check_request>(
						VBD_CHECK_DONE, progress, snap.max_level, _sb_slot.degree - 1,
						snap.nr_of_leaves, Type_1_node { snap.pba, snap.gen, snap.hash });
					if (VERBOSE_CHECK)
						log("  check snap ", _snap_idx, " (", snap, ")");
				} else {
					_sb_slot_state = VBD_CHECK_DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log("  skip snap ", _snap_idx,
						    " as it is unused");
				}
			} else {
				_sb_slot_state = DONE;
				_req_ptr = nullptr;
				progress = true;
				if (VERBOSE_CHECK)
					log("  skip superblock as it is unused");
			}
			break;

		case VBD_CHECK_DONE:

			if (_snap_idx < MAX_SNAP_IDX) {
				_snap_idx++;
				_sb_slot_state = READ_DONE;
				progress = true;
			} else {
				_snap_idx = 0;
				_gen_prim_blk_nr = _sb_slot.free_number;
				_generate_req<Ft_check_request>(
					FT_CHECK_DONE, progress, (Tree_level_index)_sb_slot.free_max_level,
					(Tree_degree)_sb_slot.free_degree - 1, (Number_of_leaves)_sb_slot.free_leaves,
					Type_1_node { _sb_slot.free_number, _sb_slot.free_gen, _sb_slot.free_hash });
				if (VERBOSE_CHECK)
					log("  check free tree");
			}
			break;

		case FT_CHECK_DONE:

			_generate_req<Ft_check_request>(
				MT_CHECK_DONE, progress, (Tree_level_index)_sb_slot.meta_max_level,
				(Tree_degree)_sb_slot.meta_degree - 1, (Number_of_leaves)_sb_slot.meta_leaves,
				Type_1_node { _sb_slot.meta_number, _sb_slot.meta_gen, _sb_slot.meta_hash });
			if (VERBOSE_CHECK)
				log("  check meta tree");
			break;

		case MT_CHECK_DONE: _mark_req_successful(progress); break;
		default: break;
		}
		break;

	default: break;
	}
}


void Sb_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("sb check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_sb_slot_state = DONE;
	_req_ptr = nullptr;
	progress = true;
}


void Sb_check_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_sb_slot_state = DONE;
	_req_ptr = nullptr;
	progress = true;
}


void Sb_check_channel::_reset()
{
	_state = INSPECT_SBS;
	_highest_gen = 0;
	_last_sb_slot_idx = 0;
	_sb_slot_idx = 0;
	_sb_slot = { };
	_snap_idx = 0;
	_vbd = { };
	_ft = { };
	_mt = { };
	_gen_prim_blk_nr = 0;
	_gen_prim_success = false;
	_encoded_blk = { };
}


void Sb_check_channel::_request_submitted(Module_request &mod_req)
{
	_reset();
	_req_ptr = static_cast<Request *>(&mod_req);
	_sb_slot_state = INIT;
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
