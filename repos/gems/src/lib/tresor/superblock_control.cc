/*
 * \brief  Module for management of the superblocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/superblock_control.h>
#include <tresor/crypto.h>
#include <tresor/free_tree.h>
#include <tresor/hash.h>

using namespace Tresor;

Superblock_control_request::
Superblock_control_request(Module_id src_module_id, Module_channel_id src_channel_id,
                           Type type, Request_offset client_req_offset,
                           Request_tag client_req_tag, Number_of_blocks nr_of_blks,
                           Virtual_block_address vba, bool &success,
                           bool &client_req_finished, Superblock::State &sb_state,
                           Generation &gen)
:
	Module_request(src_module_id, src_channel_id, SUPERBLOCK_CONTROL), _type(type), _client_req_offset(client_req_offset),
	_client_req_tag(client_req_tag), _nr_of_blks(nr_of_blks), _vba(vba), _success(success),
	_client_req_finished(client_req_finished), _sb_state(sb_state), _gen(gen)
{ }


char const *Superblock_control_request::type_to_string(Type type)
{
	switch (type) {
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case SYNC: return "sync";
	case INITIALIZE: return "initialize";
	case DEINITIALIZE: return "deinitialize";
	case VBD_EXTENSION_STEP: return "vbd_ext_step";
	case FT_EXTENSION_STEP: return "ft_ext_step";
	case CREATE_SNAPSHOT: return "create_snap";
	case DISCARD_SNAPSHOT: return "discard_snap";
	case INITIALIZE_REKEYING: return "init_rekeying";
	case REKEY_VBA: return "rekey_vba";
	}
	ASSERT_NEVER_REACHED;
}


void Superblock_control_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("sb control: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Superblock_control_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Superblock_control_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_gen_req_success) {
		error("superblock control: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	if (_state == SECURE_SB)
		_secure_sb_state = (Secure_sb_state)state_uint;
	else
		_state = (State)state_uint;
}


bool Superblock_control::Write_vba::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		if (_attr.in_vba > attr.sb.max_vba()) {
			_helper.mark_failed(progress, "VBA greater than max VBA");
			break;
		}
		if (attr.sb.curr_snap().gen != attr.curr_gen) {
			Snapshot &snap { attr.sb.curr_snap() };
			attr.sb.curr_snap_idx = attr.sb.snapshots.alloc_idx(attr.curr_gen, attr.sb.last_secured_generation);
			attr.sb.curr_snap() = snap;
			attr.sb.curr_snap().keep = false;
		}
		Key_id key_id { attr.sb.state == Superblock::REKEYING && _attr.in_vba >= attr.sb.rekeying_vba ?
			attr.sb.previous_key.id : attr.sb.current_key.id };

		_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
		_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
		_write_vba.generate(
			_helper, WRITE_VBA, WRITE_VBA_SUCCEEDED, progress, attr.sb.snapshots.items[attr.sb.curr_snap_idx], attr.sb.snapshots,
			*_ft, *_mt, _attr.in_vba, key_id, attr.sb.previous_key.id, attr.sb.degree, attr.sb.max_vba(), _attr.in_client_req_offset,
			_attr.in_client_req_tag, attr.curr_gen, attr.sb.last_secured_generation, attr.sb.state == Superblock::REKEYING,
			attr.sb.rekeying_vba);

		if (VERBOSE_WRITE_VBA)
			log("write vba ", _attr.in_vba, ": snap ", attr.sb.curr_snap_idx, " key ", key_id, " gen ", attr.curr_gen);

		break;
	}
	case WRITE_VBA: progress |= _write_vba.execute(attr.vbd, attr.client_data, attr.block_io, attr.free_tree, attr.meta_tree, attr.crypto); break;
	case WRITE_VBA_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}

void Superblock_control_channel::_do_write_vba(Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Free_tree &free_tree, Meta_tree &meta_tree, Crypto &crypto, bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (req._vba > _sb.max_vba()) {
			_mark_req_failed(progress, "VBA greater than max VBA");
			break;
		}
		if (_sb.curr_snap().gen != _curr_gen) {
			Snapshot &snap { _sb.curr_snap() };
			_sb.curr_snap_idx = _sb.snapshots.alloc_idx(_curr_gen, _sb.last_secured_generation);
			_sb.curr_snap() = snap;
			_sb.curr_snap().keep = false;
		}
		Key_id key_id { _sb.state == Superblock::REKEYING && req._vba >= _sb.rekeying_vba ?
			_sb.previous_key.id : _sb.current_key.id };

		_ft.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree, _sb.free_leaves);
		_mt.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves);
		_write_vba.generate(
			*this, WRITE_VBA, WRITE_VBA_SUCCEEDED, progress, _sb.snapshots.items[_sb.curr_snap_idx], _sb.snapshots,
			*_ft, *_mt, req._vba, key_id, _sb.previous_key.id, _sb.degree, _sb.max_vba(), _req_ptr->_client_req_offset,
			_req_ptr->_client_req_tag, _curr_gen, _sb.last_secured_generation, _sb.state == Superblock::REKEYING,
			_sb.rekeying_vba);

		if (VERBOSE_READ_VBA)
			log("read vba ", req._vba, ": snap ", _sb.curr_snap_idx, " key ", key_id, " gen ", _curr_gen);

		break;
	}
	case WRITE_VBA: progress |= _write_vba.execute(vbd, client_data, block_io, free_tree, meta_tree, crypto); break;
	case WRITE_VBA_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_channel::
_do_read_vba(Virtual_block_device &vbd, Client_data_interface &client_data, Block_io &block_io, Crypto &crypto, bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (req._vba > _sb.max_vba()) {
			_mark_req_failed(progress, "VBA greater than max VBA");
			break;
		}
		Key_id key_id { _sb.state == Superblock::REKEYING && req._vba >= _sb.rekeying_vba ?
			_sb.previous_key.id : _sb.current_key.id };

		_read_vba.generate(
			*this, READ_VBA, READ_VBA_SUCCEEDED, progress, _sb.snapshots.items[_sb.curr_snap_idx], req._vba, key_id,
			_sb.degree, _req_ptr->_client_req_offset, _req_ptr->_client_req_tag);

		if (VERBOSE_READ_VBA)
			log("read vba ", req._vba, ": snap ", _sb.curr_snap_idx, " key ", key_id, " gen ", _curr_gen);

		break;
	}
	case READ_VBA: progress |= _read_vba.execute(vbd, client_data, block_io, crypto); break;
	case READ_VBA_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


bool Superblock_control::Read_vba::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		if (_attr.in_vba > attr.sb.max_vba()) {
			_helper.mark_failed(progress, "VBA greater than max VBA");
			break;
		}
		Key_id key_id { attr.sb.state == Superblock::REKEYING && _attr.in_vba >= attr.sb.rekeying_vba ?
			attr.sb.previous_key.id : attr.sb.current_key.id };

		_read_vba.generate(
			_helper, READ_VBA, READ_VBA_SUCCEEDED, progress, attr.sb.snapshots.items[attr.sb.curr_snap_idx], _attr.in_vba, key_id,
			attr.sb.degree, _attr.in_client_req_offset, _attr.in_client_req_tag);

		if (VERBOSE_READ_VBA)
			log("read vba ", _attr.in_vba, ": snap ", attr.sb.curr_snap_idx, " key ", key_id, " gen ", attr.curr_gen);

		break;
	}
	case READ_VBA: progress |= _read_vba.execute(attr.vbd, attr.client_data, attr.block_io, attr.crypto); break;
	case READ_VBA_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


bool Superblock_control::Extend_vbd::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		_num_pbas = _attr.in_num_pbas;
		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		Physical_block_address last_used_pba { attr.sb.first_pba + (attr.sb.nr_of_pbas - 1) };
		Number_of_blocks nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (_num_pbas > nr_of_unused_pbas) {
			_helper.mark_failed(progress, "check number of unused blocks");
			break;
		}
		if (attr.sb.state == Superblock::NORMAL) {

			_attr.out_extension_finished = false;
			attr.sb.state = Superblock::EXTENDING_VBD;
			attr.sb.resizing_nr_of_pbas = _num_pbas;
			attr.sb.resizing_nr_of_leaves = 0;
			_pba = last_used_pba + 1;
			if (VERBOSE_VBD_EXTENSION)
				log("vbd ext init: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)attr.sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)attr.sb.resizing_nr_of_leaves);

			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
			break;

		} else if (attr.sb.state == Superblock::EXTENDING_VBD) {

			_pba = last_used_pba + 1;
			_num_pbas = attr.sb.resizing_nr_of_pbas;

			if (VERBOSE_VBD_EXTENSION)
				log("vbd ext step: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)attr.sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)attr.sb.resizing_nr_of_leaves);

			_num_pbas = attr.sb.resizing_nr_of_pbas;
			_pba = attr.sb.first_pba + attr.sb.nr_of_pbas;

			_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
			_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
			_extend_vbd.generate(
				_helper, EXTEND_VBD, EXTEND_VBD_SUCCEEDED, progress, _nr_of_leaves, attr.sb.snapshots,
				attr.sb.degree, attr.curr_gen, attr.sb.last_secured_generation, _pba, _num_pbas, *_ft,
				*_mt, attr.sb.degree, attr.sb.max_vba(), attr.sb.previous_key.id, attr.sb.current_key.id,
				attr.sb.state == Superblock::REKEYING, attr.sb.rekeying_vba);

		} else
			_helper.mark_failed(progress, "check superblock state");

		break;
	}
	case EXTEND_VBD: progress |= _extend_vbd.execute(attr.vbd, attr.block_io, attr.free_tree, attr.meta_tree); break;
	case EXTEND_VBD_SUCCEEDED:
	{
		if (_num_pbas >= attr.sb.resizing_nr_of_pbas) {
			_helper.mark_failed(progress, "check number of pbas");
			break;
		}
		Number_of_blocks const nr_of_added_pbas { attr.sb.resizing_nr_of_pbas - _num_pbas };
		Physical_block_address const new_first_unused_pba { attr.sb.first_pba + (attr.sb.nr_of_pbas + nr_of_added_pbas) };
		if (_pba != new_first_unused_pba) {
			_helper.mark_failed(progress, "check new first unused pba");
			break;
		}
		attr.sb.nr_of_pbas = attr.sb.nr_of_pbas + nr_of_added_pbas;
		attr.sb.resizing_nr_of_pbas = _num_pbas;
		attr.sb.resizing_nr_of_leaves += _nr_of_leaves;
		attr.sb.curr_snap_idx = attr.sb.snapshots.newest_snap_idx();

		if (!_num_pbas) {
			attr.sb.state = Superblock::NORMAL;
			_attr.out_extension_finished = true;
		}
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;
	}
	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


void Superblock_control_channel::_tree_ext_step(Block_io &block_io, Trust_anchor &trust_anchor, Free_tree &free_tree, Meta_tree &meta_tree, Virtual_block_device &vbd, Superblock::State sb_state, bool verbose, bool &progress)
{
	Request &req { *_req_ptr };

	String<4> const tree_name =
		sb_state == Superblock::EXTENDING_VBD ? "vbd" :
		sb_state == Superblock::EXTENDING_FT ? "ft" : "?";

	switch (_state) {
	case REQ_SUBMITTED:
	{
		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		Physical_block_address const last_used_pba { _sb.first_pba + (_sb.nr_of_pbas - 1) };
		Number_of_blocks const nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (req._nr_of_blks > nr_of_unused_pbas) {
			_mark_req_failed(progress, "check number of unused blocks");
			break;
		}

		if (_sb.state == Superblock::NORMAL) {

			req._client_req_finished = false;
			_sb.state = sb_state;
			_sb.resizing_nr_of_pbas = req._nr_of_blks;
			_sb.resizing_nr_of_leaves = 0;
			_pba = last_used_pba + 1;
			if (verbose)
				log(tree_name, " ext init: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			_start_secure_sb(progress);
			break;

		} else if (_sb.state == sb_state) {

			_pba = last_used_pba + 1;
			req._nr_of_blks = _sb.resizing_nr_of_pbas;

			if (verbose)
				log(tree_name, " ext step: pbas ", _pba, "..",
				    _pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			req._nr_of_blks = _sb.resizing_nr_of_pbas;
			_pba = _sb.first_pba + _sb.nr_of_pbas;

			_ft.construct(
				_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree,
				_sb.free_leaves);

			_mt.construct(
				_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree,
				_sb.meta_leaves);

			switch (sb_state) {
			case Superblock::EXTENDING_VBD:
				_extend_vbd.generate(
					*this, EXTEND_VBD, EXTEND_TREE_SUCCEEDED, progress, _nr_of_leaves, _sb.snapshots,
					_sb.degree, _curr_gen, _sb.last_secured_generation, _pba, req._nr_of_blks, *_ft,
					*_mt, _sb.degree, _sb.max_vba(), _sb.previous_key.id, _sb.current_key.id,
					_sb.state == Superblock::REKEYING, _sb.rekeying_vba);
				break;
			case Superblock::EXTENDING_FT:
				_extend_free_tree.generate(*this, EXTEND_FREE_TREE, EXTEND_TREE_SUCCEEDED, progress, _curr_gen, *_ft, *_mt, _pba, req._nr_of_blks);
				break;
			default: ASSERT_NEVER_REACHED;
			}
		} else
			_mark_req_failed(progress, "check superblock state");

		break;
	}
	case EXTEND_VBD: progress |= _extend_vbd.execute(vbd, block_io, free_tree, meta_tree); break;
	case EXTEND_FREE_TREE: progress |= _extend_free_tree.execute(free_tree, block_io, meta_tree); break;
	case EXTEND_TREE_SUCCEEDED:
	{
		if (req._nr_of_blks >= _sb.resizing_nr_of_pbas) {
			_mark_req_failed(progress, "check number of pbas");
			break;
		}
		Number_of_blocks const nr_of_added_pbas { _sb.resizing_nr_of_pbas - req._nr_of_blks };
		Physical_block_address const new_first_unused_pba { _sb.first_pba + (_sb.nr_of_pbas + nr_of_added_pbas) };
		if (_pba != new_first_unused_pba) {
			_mark_req_failed(progress, "check new first unused pba");
			break;
		}
		_sb.nr_of_pbas = _sb.nr_of_pbas + nr_of_added_pbas;
		_sb.resizing_nr_of_pbas = req._nr_of_blks;
		_sb.resizing_nr_of_leaves += _nr_of_leaves;

		if (tree_name == "vbd")
			_sb.curr_snap_idx = _sb.snapshots.newest_snap_idx();

		if (!req._nr_of_blks) {
			_sb.state = Superblock::NORMAL;
			req._client_req_finished = true;
		}
		_start_secure_sb(progress);
		break;
	}
	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


bool Superblock_control::Continue_rekeying::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		if (attr.sb.state != Superblock::REKEYING) {
			_helper.mark_failed(progress, "check superblock state");
			break;
		}
		_ft.construct(attr.sb.free_number, attr.sb.free_gen, attr.sb.free_hash, attr.sb.free_max_level, attr.sb.free_degree, attr.sb.free_leaves);
		_mt.construct(attr.sb.meta_number, attr.sb.meta_gen, attr.sb.meta_hash, attr.sb.meta_max_level, attr.sb.meta_degree, attr.sb.meta_leaves);
		_rekey_vba.generate(
			_helper, REKEY_VBA, REKEY_VBA_SUCCEEDED, progress, attr.sb.snapshots, *_ft, *_mt, attr.sb.rekeying_vba,
			attr.curr_gen, attr.sb.last_secured_generation, attr.sb.current_key.id, attr.sb.previous_key.id, attr.sb.degree, attr.sb.max_vba());


		if (VERBOSE_REKEYING)
			log("rekey vba ", attr.sb.rekeying_vba, ":\n  update vbd: keys ", attr.sb.previous_key.id, ",", attr.sb.current_key.id,
			    " generations ", attr.sb.last_secured_generation, ",", attr.curr_gen);
		break;

	case REKEY_VBA: progress |= _rekey_vba.execute(attr.vbd, attr.block_io, attr.crypto, attr.free_tree, attr.meta_tree); break;
	case REKEY_VBA_SUCCEEDED:
	{
		Number_of_leaves max_nr_of_leaves { 0 };
		for (Snapshot const &snap : attr.sb.snapshots.items) {
			if (snap.valid && max_nr_of_leaves < snap.nr_of_leaves)
				max_nr_of_leaves = snap.nr_of_leaves;
		}
		if (attr.sb.rekeying_vba < max_nr_of_leaves - 1) {
			attr.sb.rekeying_vba++;
			_attr.out_rekeying_finished = false;
			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", attr.curr_gen);
		} else {
			_remove_key.generate(_helper, REMOVE_KEY, REMOVE_KEY_SUCCEEDED, progress, attr.sb.previous_key.id);
			if (VERBOSE_REKEYING)
				log("  remove key ", attr.sb.previous_key.id);
		}
		break;
	}
	case REMOVE_KEY: progress |= _remove_key.execute(attr.crypto); break;
	case REMOVE_KEY_SUCCEEDED:

		attr.sb.previous_key = { };
		attr.sb.state = Superblock::NORMAL;
		_attr.out_rekeying_finished = true;
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", attr.curr_gen);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


void Superblock_control_channel::_do_rekey_vba(Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor, Free_tree &free_tree, Meta_tree &meta_tree, Virtual_block_device &vbd, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (_sb.state != Superblock::REKEYING) {
			_mark_req_failed(progress, "check superblock state");
			break;
		}
		_ft.construct(_sb.free_number, _sb.free_gen, _sb.free_hash, _sb.free_max_level, _sb.free_degree, _sb.free_leaves);
		_mt.construct(_sb.meta_number, _sb.meta_gen, _sb.meta_hash, _sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves);
		_rekey_vba.generate(
			*this, REKEY_VBA, REKEY_VBA_SUCCEEDED, progress, _sb.snapshots, *_ft, *_mt, _sb.rekeying_vba,
			_curr_gen, _sb.last_secured_generation, _sb.current_key.id, _sb.previous_key.id, _sb.degree, _sb.max_vba());


		if (VERBOSE_REKEYING)
			log("rekey vba ", _sb.rekeying_vba, ":\n  update vbd: keys ", _sb.previous_key.id, ",", _sb.current_key.id,
			    " generations ", _sb.last_secured_generation, ",", _curr_gen);
		break;

	case REKEY_VBA: progress |= _rekey_vba.execute(vbd, block_io, crypto, free_tree, meta_tree); break;
	case REKEY_VBA_SUCCEEDED:
	{
		Number_of_leaves max_nr_of_leaves { 0 };
		for (Snapshot const &snap : _sb.snapshots.items) {
			if (snap.valid && max_nr_of_leaves < snap.nr_of_leaves)
				max_nr_of_leaves = snap.nr_of_leaves;
		}
		if (_sb.rekeying_vba < max_nr_of_leaves - 1) {
			_sb.rekeying_vba++;
			_req_ptr->_client_req_finished = false;
			_start_secure_sb(progress);
			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", _curr_gen);
		} else {
			_remove_key.generate(*this, REMOVE_KEY, REMOVE_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.id);
			if (VERBOSE_REKEYING)
				log("  remove key ", _sb.previous_key.id);
		}
		break;
	}
	case REMOVE_KEY: progress |= _remove_key.execute(crypto); break;
	case REMOVE_PREV_KEY_SUCCEEDED:

		_sb.previous_key = { };
		_sb.state = Superblock::NORMAL;
		_req_ptr->_client_req_finished = true;
		_start_secure_sb(progress);
		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);
		break;

	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_channel::_start_secure_sb(bool &progress)
{
	_state = SECURE_SB;
	_secure_sb_state = STARTED;
	progress = true;
}


bool Superblock_control::Secure_superblock::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.curr_snap().gen = attr.curr_gen;
		_sb_ciphertext.copy_all_but_key_values_from(attr.sb);
		_encrypt_key.generate(
			_helper, ENCRYPT_KEY, ENCRYPT_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key.value, _sb_ciphertext.current_key.value);
		break;

	case ENCRYPT_KEY: progress |= _encrypt_key.execute(attr.trust_anchor); break;
	case ENCRYPT_CURR_KEY_SUCCEEDED:

		if (attr.sb.state == Superblock::REKEYING)
			_encrypt_key.generate(
				_helper, ENCRYPT_KEY, ENCRYPT_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			_sb_ciphertext.encode_to_blk(_blk);
			_write_block.generate(_helper, WRITE_BLOCK, WRITE_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
		}
		break;

	case ENCRYPT_PREV_KEY_SUCCEEDED:

		_sb_ciphertext.encode_to_blk(_blk);
		_write_block.generate(_helper, WRITE_BLOCK, WRITE_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
		break;

	case WRITE_BLOCK: progress |= _write_block.execute(attr.block_io); break;
	case WRITE_BLOCK_SUCCEEDED: _sync_block_io.generate(_helper, SYNC_BLOCK_IO, SYNC_BLOCK_IO_SUCCEEDED, progress); break;
	case SYNC_BLOCK_IO: progress |= _sync_block_io.execute(attr.block_io); break;
	case SYNC_BLOCK_IO_SUCCEEDED:
	{
		_sb_ciphertext.encode_to_blk(_blk);
		calc_hash(_blk, _hash);
		_write_sb_hash.generate(_helper, WRITE_SB_HASH, WRITE_SB_HASH_SUCCEEDED, progress, _hash);
		if (attr.sb_idx < MAX_SUPERBLOCK_INDEX)
			attr.sb_idx++;
		else
			attr.sb_idx = 0;

		_gen = attr.curr_gen;
		attr.curr_gen++;
		break;
	}
	case WRITE_SB_HASH: progress |= _write_sb_hash.execute(attr.trust_anchor); break;
	case WRITE_SB_HASH_SUCCEEDED:

		attr.sb.last_secured_generation = _gen;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


void Superblock_control_channel::_secure_sb(Block_io &block_io, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_secure_sb_state) {
	case STARTED:

		_sb.curr_snap().gen = _curr_gen;
		_sb_ciphertext.copy_all_but_key_values_from(_sb);
		_encrypt_key.generate(
			*this, ENCRYPT_KEY, ENCRYPT_CURR_KEY_SUCCEEDED, progress, _sb.current_key.value, _sb_ciphertext.current_key.value);
		break;

	case ENCRYPT_KEY: progress |= _encrypt_key.execute(trust_anchor); break;
	case ENCRYPT_CURR_KEY_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING)
			_encrypt_key.generate(
				*this, ENCRYPT_KEY, ENCRYPT_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			_sb_ciphertext.encode_to_blk(_blk);
			_write_block.generate(*this, WRITE_BLOCK, WRITE_SB_SUCCEEDED, progress, _sb_idx, _blk);
		}
		break;

	case ENCRYPT_PREV_KEY_SUCCEEDED:

		_sb_ciphertext.encode_to_blk(_blk);
		_write_block.generate(*this, WRITE_BLOCK, WRITE_SB_SUCCEEDED, progress, _sb_idx, _blk);
		break;

	case WRITE_BLOCK: progress |= _write_block.execute(block_io); break;
	case WRITE_SB_SUCCEEDED: _sync_block_io.generate(*this, SYNC_BLOCK_IO, SYNC_BLOCK_IO_SUCCEEDED, progress); break;
	case SYNC_BLOCK_IO: progress |= _sync_block_io.execute(block_io); break;
	case SYNC_BLOCK_IO_SUCCEEDED:
	{
		_sb_ciphertext.encode_to_blk(_blk);
		calc_hash(_blk, _hash);
		_write_sb_hash.generate(*this, WRITE_SB_HASH, WRITE_SB_HASH_SUCCEEDED, progress, _hash);
		if (_sb_idx < MAX_SUPERBLOCK_INDEX)
			_sb_idx++;
		else
			_sb_idx = 0;

		_gen = _curr_gen;
		_curr_gen++;
		break;
	}
	case WRITE_SB_HASH: progress |= _write_sb_hash.execute(trust_anchor); break;
	case WRITE_SB_HASH_SUCCEEDED:

		_sb.last_secured_generation = _gen;
		_state = SECURE_SB_SUCCEEDED;
		progress = true;
		break;

	default: break;
	}
}

bool Superblock_control::Start_rekeying::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		if (attr.sb.state != Superblock::NORMAL) {
			_helper.mark_failed(progress, "check superblock state");
			break;
		}
		attr.sb.state = Superblock::REKEYING;
		attr.sb.rekeying_vba = 0;
		attr.sb.previous_key = attr.sb.current_key;
		attr.sb.current_key.id++;
		_generate_key.generate(_helper, GENERATE_KEY, GENERATE_KEY_SUCCEEDED, progress, attr.sb.current_key.value);
		break;

	case GENERATE_KEY: progress |= _generate_key.execute(attr.trust_anchor); break;
	case GENERATE_KEY_SUCCEEDED:

		_add_key.generate(_helper, ADD_KEY, ADD_KEY_SUCCEEDED, progress, attr.sb.current_key);
		if (VERBOSE_REKEYING)
			log("start rekeying:\n  update sb: keys ", attr.sb.previous_key.id, ",", attr.sb.current_key.id);
		break;

	case ADD_KEY: progress |= _add_key.execute(attr.crypto); break;
	case ADD_KEY_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", attr.curr_gen);
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


void Superblock_control_channel::_init_rekeying(Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (_sb.state != Superblock::NORMAL) {
			_mark_req_failed(progress, "check superblock state");
			break;
		}
		_sb.state = Superblock::REKEYING;
		_sb.rekeying_vba = 0;
		_sb.previous_key = _sb.current_key;
		_sb.current_key.id++;
		_generate_key.generate(*this, GENERATE_KEY, GENERATE_KEY_SUCCEEDED, progress, _sb.current_key.value);
		break;

	case GENERATE_KEY: progress |= _generate_key.execute(trust_anchor); break;
	case GENERATE_KEY_SUCCEEDED:

		_add_key.generate(*this, ADD_KEY, ADD_CURR_KEY_SUCCEEDED, progress, _sb.current_key);
		if (VERBOSE_REKEYING)
			log("start rekeying:\n  update sb: keys ", _sb.previous_key.id, ",", _sb.current_key.id);
		break;

	case ADD_KEY: progress |= _add_key.execute(crypto); break;
	case ADD_CURR_KEY_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}

bool Superblock_control::Discard_snapshot::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		for (Snapshot &snap : attr.sb.snapshots.items)
			if (snap.valid && snap.gen == _attr.in_gen && snap.keep)
				snap.keep = false;

		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


void Superblock_control_channel::_discard_snap(Block_io &block_io, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		for (Snapshot &snap : _sb.snapshots.items)
			if (snap.valid && snap.gen == _req_ptr->_gen && snap.keep)
				snap.keep = false;

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED:

		_req_ptr->_gen = _gen;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


bool Superblock_control::Create_snapshot::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:
	{
		Snapshot &snap = attr.sb.curr_snap();
		if (snap.keep) {
			_attr.out_gen = snap.gen;
			_helper.mark_succeeded(progress);
		} else {
			snap.keep = true;
			attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
			_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		}
		break;
	}
	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED:

		_attr.out_gen = attr.sb.curr_snap().gen;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}

void Superblock_control_channel::_create_snap(Block_io &block_io, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		if (_sb.curr_snap().keep) {
			_req_ptr->_gen = _sb.curr_snap().gen;
			_mark_req_successful(progress);
		} else {
			_sb.curr_snap().keep = true;
			_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
			_start_secure_sb(progress);
		}
		break;

	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED:

		_req_ptr->_gen = _gen;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


bool Superblock_control::Synchronize::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		attr.sb.last_secured_generation = attr.curr_gen;
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _helper.mark_succeeded(progress); break;
	default: break;
	}
	return progress;
}


void Superblock_control_channel::_sync(Block_io &block_io, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control_request::print(Output &out) const
{
	Genode::print(out, type_to_string(_type));
	switch (_type) {
	case REKEY_VBA:
	case READ_VBA:
	case WRITE_VBA: Genode::print(out, " ", _vba); break;
	default: break;
	}
}


bool Superblock_control::Initialize::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT: _read_sb_hash.generate(_helper, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, progress, _hash); break;
	case READ_SB_HASH: progress |= _read_sb_hash.execute(attr.trust_anchor); break;
	case READ_SB_HASH_SUCCEEDED:

		attr.sb_idx = 0;
		_read_block.generate(_helper, READ_BLOCK, READ_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
		break;

	case READ_BLOCK: progress |= _read_block.execute(attr.block_io); break;
	case READ_BLOCK_SUCCEEDED:

		_sb_ciphertext.decode_from_blk(_blk);
		if (check_hash(_blk, _hash)) {
			_gen = _sb_ciphertext.snapshots.items[_sb_ciphertext.snapshots.newest_snap_idx()].gen;
			attr.sb.copy_all_but_key_values_from(_sb_ciphertext);
			_decrypt_key.generate(_helper, DECRYPT_KEY, DECRYPT_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key.value, _sb_ciphertext.current_key.value);
		} else
			if (attr.sb_idx < MAX_SUPERBLOCK_INDEX) {
				attr.sb_idx++;
				_read_block.generate(_helper, READ_BLOCK, READ_BLOCK_SUCCEEDED, progress, attr.sb_idx, _blk);
			} else
				_helper.mark_failed(progress, "superblock not found");
		break;

	case DECRYPT_KEY: progress |= _decrypt_key.execute(attr.trust_anchor); break;
	case DECRYPT_CURR_KEY_SUCCEEDED: _add_key.generate(_helper, ADD_KEY, ADD_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key); break;
	case ADD_KEY: progress |= _add_key.execute(attr.crypto); break;
	case ADD_CURR_KEY_SUCCEEDED:

		if (_sb_ciphertext.state == Superblock::REKEYING)
			_decrypt_key.generate(_helper, DECRYPT_KEY, DECRYPT_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			attr.curr_gen = _gen + 1;
			_attr.out_sb_state = attr.sb.state;
			_helper.mark_succeeded(progress);
		}
		break;

	case DECRYPT_PREV_KEY_SUCCEEDED: _add_key.generate(_helper, ADD_KEY, ADD_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key); break;
	case ADD_PREV_KEY_SUCCEEDED:

		attr.curr_gen = _gen + 1;
		_attr.out_sb_state = attr.sb.state;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


void Superblock_control_channel::_initialize(Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_read_sb_hash.generate(*this, READ_SB_HASH, READ_SB_HASH_SUCCEEDED, progress, _hash);
		break;

	case READ_SB_HASH: progress |= _read_sb_hash.execute(trust_anchor); break;
	case READ_SB_HASH_SUCCEEDED:

		_sb_idx = 0;
		_read_block.generate(*this, READ_BLOCK, READ_SB_SUCCEEDED, progress, _sb_idx, _blk);
		break;

	case READ_BLOCK: progress |= _read_block.execute(block_io); break;
	case READ_SB_SUCCEEDED:

		_sb_ciphertext.decode_from_blk(_blk);
		if (check_hash(_blk, _hash)) {
			_gen = _sb_ciphertext.snapshots.items[_sb_ciphertext.snapshots.newest_snap_idx()].gen;
			_sb.copy_all_but_key_values_from(_sb_ciphertext);
			_decrypt_key.generate(*this, DECRYPT_KEY, DECRYPT_CURR_KEY_SUCCEEDED, progress, _sb.current_key.value, _sb_ciphertext.current_key.value);
		} else
			if (_sb_idx < MAX_SUPERBLOCK_INDEX) {
				_sb_idx++;
				_read_block.generate(*this, READ_BLOCK, READ_SB_SUCCEEDED, progress, _sb_idx, _blk);
			} else
				_mark_req_failed(progress, "superblock not found");
		break;

	case DECRYPT_KEY: progress |= _decrypt_key.execute(trust_anchor); break;
	case DECRYPT_CURR_KEY_SUCCEEDED: _add_key.generate(*this, ADD_KEY, ADD_CURR_KEY_SUCCEEDED, progress, _sb.current_key); break;
	case ADD_KEY: progress |= _add_key.execute(crypto); break;
	case ADD_CURR_KEY_SUCCEEDED:

		if (_sb_ciphertext.state == Superblock::REKEYING)
			_decrypt_key.generate(*this, DECRYPT_KEY, DECRYPT_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.value, _sb_ciphertext.previous_key.value);
		else {
			_curr_gen = _gen + 1;
			_req_ptr->_sb_state = _sb.state;
			_mark_req_successful(progress);
		}
		break;

	case DECRYPT_PREV_KEY_SUCCEEDED: _add_key.generate(*this, ADD_KEY, ADD_PREV_KEY_SUCCEEDED, progress, _sb.previous_key); break;
	case ADD_PREV_KEY_SUCCEEDED:

		_curr_gen = _gen + 1;
		_req_ptr->_sb_state = _sb.state;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control_channel::_deinitialize(Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor, bool &progress)
{
	switch (_state) {
	case REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_start_secure_sb(progress);
		break;

	case SECURE_SB: _secure_sb(block_io, trust_anchor, progress); break;
	case SECURE_SB_SUCCEEDED: _remove_key.generate(*this, REMOVE_KEY, REMOVE_CURR_KEY_SUCCEEDED, progress, _sb.current_key.id); break;
	case REMOVE_CURR_KEY_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING)
			_remove_key.generate(*this, REMOVE_KEY, REMOVE_PREV_KEY_SUCCEEDED, progress, _sb.previous_key.id);
		else
			_mark_req_successful(progress);
		break;

	case REMOVE_KEY: progress |= _remove_key.execute(crypto); break;
	case REMOVE_PREV_KEY_SUCCEEDED:

		_sb.state = Superblock::INVALID;
		_mark_req_successful(progress);
		break;

	default: break;
	}
}

bool Superblock_control::Deinitialize::execute(Execute_attr const &attr)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		attr.sb.snapshots.discard_disposable_snapshots(attr.sb.last_secured_generation, attr.curr_gen);
		attr.sb.last_secured_generation = attr.curr_gen;
		_secure_sb.generate(_helper, SECURE_SB, SECURE_SB_SUCCEEDED, progress);
		break;

	case SECURE_SB: progress |= _secure_sb.execute(attr.sb_control, attr.block_io, attr.trust_anchor); break;
	case SECURE_SB_SUCCEEDED: _remove_key.generate(_helper, REMOVE_KEY, REMOVE_CURR_KEY_SUCCEEDED, progress, attr.sb.current_key.id); break;
	case REMOVE_CURR_KEY_SUCCEEDED:

		if (attr.sb.state == Superblock::REKEYING)
			_remove_key.generate(_helper, REMOVE_KEY, REMOVE_PREV_KEY_SUCCEEDED, progress, attr.sb.previous_key.id);
		else
			_helper.mark_succeeded(progress);
		break;

	case REMOVE_KEY: progress |= _remove_key.execute(attr.crypto); break;
	case REMOVE_PREV_KEY_SUCCEEDED:

		attr.sb.state = Superblock::INVALID;
		_helper.mark_succeeded(progress);
		break;

	default: break;
	}
	return progress;
}


void Superblock_control_channel::execute(Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor, Free_tree &free_tree, Meta_tree &meta_tree, Virtual_block_device &vbd, Client_data_interface &client_data, bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::READ_VBA: _do_read_vba(vbd, client_data, block_io, crypto, progress); break;
	case Request::WRITE_VBA: _do_write_vba(vbd, client_data, block_io, free_tree, meta_tree, crypto, progress); break;
	case Request::SYNC: _sync(block_io, trust_anchor, progress); break;
	case Request::INITIALIZE_REKEYING: _init_rekeying(block_io, crypto, trust_anchor, progress); break;
	case Request::REKEY_VBA: _do_rekey_vba(block_io, crypto, trust_anchor, free_tree, meta_tree, vbd, progress); break;
	case Request::VBD_EXTENSION_STEP: _tree_ext_step(block_io, trust_anchor, free_tree, meta_tree, vbd, Superblock::EXTENDING_VBD, VERBOSE_VBD_EXTENSION, progress); break;
	case Request::FT_EXTENSION_STEP: _tree_ext_step(block_io, trust_anchor, free_tree, meta_tree, vbd, Superblock::EXTENDING_FT, VERBOSE_FT_EXTENSION, progress); break;
	case Request::CREATE_SNAPSHOT: _create_snap(block_io, trust_anchor, progress); break;
	case Request::DISCARD_SNAPSHOT: _discard_snap(block_io, trust_anchor, progress); break;
	case Request::INITIALIZE: _initialize(block_io, crypto, trust_anchor, progress); break;
	case Request::DEINITIALIZE: _deinitialize(block_io, crypto, trust_anchor, progress); break;
	}
}


void Superblock_control::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(_block_io, _crypto, _trust_anchor, _free_tree, _meta_tree, _vbd, _client_data, progress); });
}


Snapshots_info Superblock_control::snapshots_info() const
{
	Snapshots_info info { };
	if (_sb.valid()) {
		for (Snapshot_index idx { 0 }; idx < MAX_NR_OF_SNAPSHOTS; idx++) {
			Snapshot const &snap { _sb.snapshots.items[idx] };
			if (snap.valid && snap.keep)
				info.generations[idx] = snap.gen;
		}
	}
	return info;
}


Superblock_info Superblock_control::sb_info() const
{
	if (!_sb.valid())
		return Superblock_info { };

	return Superblock_info {
		true, _sb.state == Superblock::REKEYING, _sb.state == Superblock::EXTENDING_FT,
		_sb.state == Superblock::EXTENDING_VBD };
}


void Superblock_control_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request *>(&req);
	_state = REQ_SUBMITTED;
}


Superblock_control::Superblock_control(Block_io &block_io, Crypto &crypto, Trust_anchor &trust_anchor, Free_tree &free_tree,
                                       Meta_tree &meta_tree, Virtual_block_device &vbd, Client_data_interface &client_data)
:
	_block_io(block_io),
	_crypto(crypto),
	_trust_anchor(trust_anchor),
	_free_tree(free_tree),
	_meta_tree(meta_tree),
	_vbd(vbd), _client_data(client_data)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, _sb, _sb_idx, _curr_gen);
		add_channel(*chan);
	}
}


Superblock_control_channel::
Superblock_control_channel(Module_channel_id id, Superblock &sb, Superblock_index &sb_idx, Generation &curr_gen)
:
	Module_channel(SUPERBLOCK_CONTROL, id), _sb(sb), _sb_idx(sb_idx), _curr_gen(curr_gen)
{ }
