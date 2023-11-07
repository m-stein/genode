/*
 * \brief  Module for initializing the superblocks of a new Tresor
 * \author Josef Soentgen
 * \date   2023-03-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/hash.h>
#include <tresor/block_io.h>
#include <tresor/vbd_initializer.h>
#include <tresor/ft_initializer.h>
#include <tresor/trust_anchor.h>
#include <tresor/sb_initializer.h>

using namespace Tresor;

Sb_initializer_request::
Sb_initializer_request(Module_id src_mod, Module_request_id src_chan, Tree_level_index vbd_max_lvl,
                       Tree_degree vbd_degree, Number_of_leaves vbd_num_leaves, Tree_level_index ft_max_lvl,
                       Tree_degree ft_degree, Number_of_leaves ft_num_leaves, Tree_level_index mt_max_lvl,
                       Tree_degree mt_degree, Number_of_leaves mt_num_leaves, Pba_allocator &pba_alloc, bool &success)
:
	Module_request { src_mod, src_chan, SB_INITIALIZER }, _vbd_max_lvl { vbd_max_lvl },
	_vbd_degree { vbd_degree }, _vbd_num_leaves { vbd_num_leaves }, _ft_max_lvl { ft_max_lvl },
	_ft_degree { ft_degree }, _ft_num_leaves { ft_num_leaves }, _mt_max_lvl { mt_max_lvl },
	_mt_degree { mt_degree }, _mt_num_leaves { mt_num_leaves }, _pba_alloc { pba_alloc }, _success { success }
{ }


void Sb_initializer_channel::_populate_sb_slot(Physical_block_address first,
                                       Number_of_blocks       num)
{
	Superblock &sb = _sb;
	Request &req { *_req_ptr };
	sb.state = Superblock::NORMAL;
	Snapshot &snap = sb.snapshots.items[0];
	snap.gen = 0;
	snap.nr_of_leaves = req._vbd_num_leaves;
	snap.max_level = req._vbd_max_lvl;
	snap.valid = true;
	snap.id = 0;
	snap.keep = false;

	sb.rekeying_vba            = 0;
	sb.resizing_nr_of_pbas     = 0;
	sb.resizing_nr_of_leaves   = 0;
	memset(&sb.previous_key, 0, sizeof(sb.previous_key));
	sb.current_key             = _key_cipher;
	sb.curr_snap_idx           = 0;
	sb.degree                  = req._vbd_degree;
	sb.first_pba               = first;
	sb.nr_of_pbas              = num;
	sb.last_secured_generation = 0;
	sb.free_max_level          = _ft->max_lvl;
	sb.free_degree             = _ft->degree;
	sb.free_leaves             = _ft->num_leaves;
	sb.meta_max_level          = _mt->max_lvl;
	sb.meta_degree             = _mt->degree;
	sb.meta_leaves             = _mt->num_leaves;
}


void Sb_initializer_channel::_execute(bool &progress)
{

	using CS = State;
	Superblock &sb { _sb };
	Request &req { *_req_ptr };

	switch (_state) {
	case CS::IN_PROGRESS:

		if (_sb_slot_index == 0) {
			Snapshot &snap = sb.snapshots.items[0];
			_vbd.construct(snap.pba, snap.gen, snap.hash, req._vbd_max_lvl, req._vbd_degree, req._vbd_num_leaves);
			generate_req<Vbd_initializer_request>(CS::VBD_REQUEST_COMPLETE, progress, *_vbd, req._pba_alloc, _generated_req_success);
			_state = REQ_GENERATED;
		} else {
			_sb.encode_to_blk(_encoded_blk);
			_generate_req<Block_io::Write>(CS::WRITE_REQUEST_COMPLETE, progress, _sb_slot_index, _encoded_blk);
		}
		progress = true;
		break;

	case CS::VBD_REQUEST_COMPLETE:

		_ft.construct(sb.free_number, sb.free_gen, sb.free_hash, req._ft_max_lvl, req._ft_degree, req._ft_num_leaves);
		generate_req<Ft_initializer_request>(CS::FT_REQUEST_COMPLETE, progress, *_ft, req._pba_alloc, _generated_req_success);
		_state = REQ_GENERATED;
		break;

	case CS::FT_REQUEST_COMPLETE:

		_mt.construct(sb.meta_number, sb.meta_gen, sb.meta_hash, req._ft_max_lvl, req._ft_degree, req._ft_num_leaves);
		generate_req<Ft_initializer_request>(CS::MT_REQUEST_COMPLETE, progress, *_mt, req._pba_alloc, _generated_req_success);
		_state = REQ_GENERATED;
		break;

	case CS::MT_REQUEST_COMPLETE:

		generate_req<Trust_anchor::Create_key>(
			CS::TA_REQUEST_CREATE_KEY_COMPLETE, progress, _key_plain.value, _generated_req_success);
		_state = REQ_GENERATED;
		break;

	case CS::TA_REQUEST_CREATE_KEY_COMPLETE:

		generate_req<Trust_anchor::Encrypt_key>(
			CS::TA_REQUEST_ENCRYPT_KEY_COMPLETE, progress, _key_plain.value, _key_cipher.value, _generated_req_success);
		_state = REQ_GENERATED;
		break;

	case CS::TA_REQUEST_ENCRYPT_KEY_COMPLETE:

		_key_cipher.id = 1;
		_populate_sb_slot(req._pba_alloc.first_pba() - NR_OF_SUPERBLOCK_SLOTS,
			req._pba_alloc.num_used_pbas() + NR_OF_SUPERBLOCK_SLOTS);

		_sb.encode_to_blk(_encoded_blk);
		calc_hash(_encoded_blk, _sb_hash);
		_generate_req<Block_io::Write>(CS::WRITE_REQUEST_COMPLETE, progress, _sb_slot_index, _encoded_blk);
		break;

	case CS::WRITE_REQUEST_COMPLETE:

		_generate_req<Block_io::Sync>(CS::SYNC_REQUEST_COMPLETE, progress);
		progress = true;
		break;

	case CS::SYNC_REQUEST_COMPLETE:

		if (_sb_slot_index == 0) {
			generate_req<Trust_anchor::Write_hash>(
				CS::TA_REQUEST_SECURE_SB_COMPLETE, progress, _sb_hash, _generated_req_success);
			_state = REQ_GENERATED;
		} else {
			_state = CS::SLOT_COMPLETE;
		}
		progress = true;
		break;

	case CS::TA_REQUEST_SECURE_SB_COMPLETE:

		_state = CS::SLOT_COMPLETE;
		progress = true;
		break;
	default:
		break;
	}
}


void Sb_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		//_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Sb_initializer_channel::_execute_init(bool &progress)
{
	switch (_state) {
	case SUBMITTED:

		_sb_slot_index = 0;
		_state = PENDING;
		progress = true;
		return;

	case PENDING:

		clean_data();
		_state = IN_PROGRESS;
		progress = true;
		return;

	case SLOT_COMPLETE:

		if (_sb_slot_index < NR_OF_SUPERBLOCK_SLOTS - 1) {
			++_sb_slot_index;
			_state = PENDING;
			progress = true;
		} else
			_mark_req_successful(progress);
		return;

	case IN_PROGRESS:
	case FT_REQUEST_COMPLETE:
	case MT_REQUEST_COMPLETE:
	case VBD_REQUEST_COMPLETE:
	case SYNC_REQUEST_COMPLETE:
	case TA_REQUEST_CREATE_KEY_COMPLETE:
	case TA_REQUEST_ENCRYPT_KEY_COMPLETE:
	case TA_REQUEST_SECURE_SB_COMPLETE:
	case WRITE_REQUEST_COMPLETE: _execute(progress); return;
	default: return;
	}
}


void Sb_initializer_channel::_mark_req_failed(bool &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Sb_initializer_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = SUBMITTED;
}


void Sb_initializer_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };

	req._success = true;

	_state = COMPLETE;
	progress = true;
}


void Sb_initializer_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	_execute_init(progress);
}


Sb_initializer::Sb_initializer()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Sb_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
