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
#include <tresor/ft_resizing.h>
#include <tresor/sha256_4k_hash.h>

using namespace Tresor;


/********************************
 ** Superblock_control_request **
 ********************************/

Superblock_control_request::
Superblock_control_request(Module_id src_module_id, Module_request_id src_request_id,
                           Type type, Request_offset client_req_offset,
                           Request_tag client_req_tag, Number_of_blocks nr_of_blks,
                           Virtual_block_address vba, bool &success,
                           bool &client_req_finished, Superblock::State &sb_state,
                           Generation &gen)
:
	Module_request { src_module_id, src_request_id, SUPERBLOCK_CONTROL }, _type { type },
	_client_req_offset { client_req_offset }, _client_req_tag { client_req_tag },
	_nr_of_blks { nr_of_blks }, _vba { vba }, _success { success },
	_client_req_finished { client_req_finished }, _sb_state { sb_state }, _gen { gen }
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
	return "?";
}


/************************
 ** Superblock_control **
 ************************/

void Superblock_control_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("sb control: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = COMPLETED;
	progress = true;
}


void Superblock_control_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = COMPLETED;
	progress = true;
}


Virtual_block_address Superblock_control::max_vba() const
{
	ASSERT(_sb.valid());
	return _sb.curr_snap().nr_of_leaves - 1;
}


Virtual_block_address Superblock_control::rekeying_vba() const
{
	return _sb.rekeying_vba;
}


Virtual_block_address Superblock_control::resizing_nr_of_pbas() const
{
	return _sb.resizing_nr_of_pbas;
}


void Superblock_control_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_prim.succ) {
		error("request_pool: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETED;
		_req_ptr = nullptr;
	} else
		_state = (State)state_uint;
}


void Superblock_control_channel::
_generate_vbd_req(Superblock_control &mod, Virtual_block_device_request::Type type,
                  State complete_state, bool &progress, Key_id key_id, Virtual_block_address vba = INVALID_VBA)
{
	Request &req { *_req_ptr };
	Superblock &sb { mod._sb };
	_state = REQ_GENERATED;
	_pba = sb.first_pba + sb.nr_of_pbas;
	generate_req<Virtual_block_device_request>(
		complete_state, progress, type, req._client_req_offset,
		req._client_req_tag, sb.last_secured_generation, sb.free_number, sb.free_gen,
		sb.free_hash, sb.free_max_level, sb.free_degree, sb.free_leaves,
		sb.meta_number, sb.meta_gen, sb.meta_hash, sb.meta_max_level, sb.meta_degree,
		sb.meta_leaves, sb.degree, mod.max_vba(), sb.state == Superblock::REKEYING, vba,
		sb.curr_snap_idx, sb.snapshots, sb.degree, sb.previous_key.id, key_id, mod._curr_gen,
		_pba, _generated_prim.succ, _nr_of_leaves, req._nr_of_blks);
}

void Superblock_control_channel::_generate_ta_req(Trust_anchor_request::Type type, State complete_state,
                                                  bool &progress, Key_value &key_ciphertext, Key_value &key_plaintext)
{
	_state = REQ_GENERATED;
	generate_req<Trust_anchor_request>(
		complete_state, progress, type, key_plaintext, key_ciphertext, _hash, Passphrase { }, _generated_prim.succ);
}


void Superblock_control_channel::_access_vba(Superblock_control &mod, Virtual_block_device_request::Type type, bool &progress)
{
	Request &req { *_req_ptr };
	Generation &curr_gen { mod._curr_gen };
	Superblock &sb { mod._sb };
	switch (_state) {
	case SUBMITTED:
	{
		sb.snapshots.discard_disposable_snapshots(sb.last_secured_generation, curr_gen);
		if (req._vba > mod.max_vba()) {
			_mark_req_failed(progress, "VBA greater than max VBA");
			break;
		}
		if (type == Virtual_block_device_request::WRITE_VBA && sb.curr_snap().gen != curr_gen) {
			Snapshot &snap { sb.curr_snap() };
			sb.curr_snap_idx = sb.snapshots.alloc_idx(curr_gen, sb.last_secured_generation);
			sb.curr_snap() = snap;
			sb.curr_snap().keep = false;
		}
		Key_id key_id { sb.state == Superblock::REKEYING && req._vba >= sb.rekeying_vba ?
			sb.previous_key.id : sb.current_key.id };

		_generate_vbd_req(mod, type, ACCESS_VBA_AT_VBD_SUCCEEDED, progress, key_id, req._vba);
		if (VERBOSE_READ_VBA)
			log("read vba ", req._vba, ": snap ", sb.curr_snap_idx, " key ", key_id, " gen ", curr_gen);

		break;
	}
	case ACCESS_VBA_AT_VBD_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Superblock_control::_init_sb_without_key_values(Superblock const &sb_in,
                                                     Superblock       &sb_out)
{
	sb_out.state                   = sb_in.state;
	sb_out.rekeying_vba            = sb_in.rekeying_vba;
	sb_out.resizing_nr_of_pbas     = sb_in.resizing_nr_of_pbas;
	sb_out.resizing_nr_of_leaves   = sb_in.resizing_nr_of_leaves;
	sb_out.first_pba               = sb_in.first_pba;
	sb_out.nr_of_pbas              = sb_in.nr_of_pbas;
	memset(&sb_out.previous_key.value, 0, sizeof(sb_out.previous_key.value));
	sb_out.previous_key.id         = sb_in.previous_key.id;
	memset(&sb_out.current_key.value,  0, sizeof(sb_out.current_key.value));
	sb_out.current_key.id          = sb_in.current_key.id;
	sb_out.snapshots               = sb_in.snapshots;
	sb_out.last_secured_generation = sb_in.last_secured_generation;
	sb_out.curr_snap_idx               = sb_in.curr_snap_idx;
	sb_out.degree                  = sb_in.degree;
	sb_out.free_gen                = sb_in.free_gen;
	sb_out.free_number             = sb_in.free_number;
	sb_out.free_hash               = sb_in.free_hash;
	sb_out.free_max_level          = sb_in.free_max_level;
	sb_out.free_degree             = sb_in.free_degree;
	sb_out.free_leaves             = sb_in.free_leaves;
	sb_out.meta_gen                = sb_in.meta_gen;
	sb_out.meta_number             = sb_in.meta_number;
	sb_out.meta_hash               = sb_in.meta_hash;
	sb_out.meta_max_level          = sb_in.meta_max_level;
	sb_out.meta_degree             = sb_in.meta_degree;
	sb_out.meta_leaves             = sb_in.meta_leaves;
}


void Superblock_control::_execute_tree_ext_step(Channel &chan,
                                                uint64_t chan_idx,
                                                Superblock::State tree_ext_sb_state,
                                                bool tree_ext_verbose,
                                                String<4> tree_name,
                                                bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::SUBMITTED:
	{
		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		Physical_block_address const last_used_pba { _sb.first_pba + (_sb.nr_of_pbas - 1) };
		Number_of_blocks const nr_of_unused_pbas { MAX_PBA - last_used_pba };

		if (req._nr_of_blks > nr_of_unused_pbas) {
			chan._mark_req_failed(progress, "check number of unused blocks");
			break;
		}
		if (_sb.state == Superblock::NORMAL) {

			req._client_req_finished = false;
			_sb.state = tree_ext_sb_state;
			_sb.resizing_nr_of_pbas = req._nr_of_blks;
			_sb.resizing_nr_of_leaves = 0;
			chan._pba = last_used_pba + 1;

			if (tree_ext_verbose)
				log(tree_name, " ext init: pbas ", chan._pba, "..",
				    chan._pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			_secure_sb_init(chan, progress);
			break;

		} else if (_sb.state == tree_ext_sb_state) {

			chan._pba = last_used_pba + 1;
			req._nr_of_blks = _sb.resizing_nr_of_pbas;

			if (tree_ext_verbose)
				log(tree_name, " ext step: pbas ", chan._pba, "..",
				    chan._pba + (Number_of_blocks)_sb.resizing_nr_of_pbas - 1,
				    " leaves ", (Number_of_blocks)_sb.resizing_nr_of_leaves);

			chan._req_ptr->_nr_of_blks = _sb.resizing_nr_of_pbas;
			chan._pba = _sb.first_pba + _sb.nr_of_pbas;
			if (tree_name == "vbd") {

				chan._generate_vbd_req(
					*this, Virtual_block_device_request::VBD_EXTENSION_STEP,
					Channel::TREE_EXT_STEP_IN_TREE_SUCCEEDED, progress, _sb.current_key.id);

			} else if (tree_name == "ft") {

				chan._state = Channel::REQ_GENERATED;
				chan._ft_root = Type_1_node { _sb.free_number, _sb.free_gen, _sb.free_hash };
				chan._ft_max_lvl = _sb.free_max_level;
				chan._ft_nr_of_leaves = _sb.free_leaves;
				chan.generate_req<Ft_resizing_request>(
					Channel::TREE_EXT_STEP_IN_TREE_SUCCEEDED, progress, Ft_resizing_request::FT_EXTENSION_STEP,
					_curr_gen, chan._ft_root, chan._ft_max_lvl, chan._ft_nr_of_leaves,
					_sb.free_degree, _sb.meta_number, _sb.meta_gen, _sb.meta_hash,
					_sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves, chan._pba,
					chan._req_ptr->_nr_of_blks, chan._generated_prim.succ);
			}

		} else
			chan._mark_req_failed(progress, "check superblock state");

		break;
	}
	case Channel::TREE_EXT_STEP_IN_TREE_SUCCEEDED:
	{
		if (req._nr_of_blks >= _sb.resizing_nr_of_pbas) {
			chan._mark_req_failed(progress, "check number of pbas");
			break;
		}
		Number_of_blocks const nr_of_added_pbas { _sb.resizing_nr_of_pbas - req._nr_of_blks };
		Physical_block_address const new_first_unused_pba { _sb.first_pba + (_sb.nr_of_pbas + nr_of_added_pbas) };

		if (chan._pba != new_first_unused_pba) {
			chan._mark_req_failed(progress, "check new first unused pba");
			break;
		}
		_sb.nr_of_pbas = _sb.nr_of_pbas + nr_of_added_pbas;
		_sb.resizing_nr_of_pbas = req._nr_of_blks;
		_sb.resizing_nr_of_leaves += chan._nr_of_leaves;

		if (tree_name == "vbd") {

			_sb.curr_snap_idx = _sb.snapshots.newest_snapshot_idx();

		} else if (tree_name == "ft") {

			_sb.free_gen = chan._ft_root.gen;
			_sb.free_number = chan._ft_root.pba;
			_sb.free_hash = chan._ft_root.hash;
			_sb.free_max_level = chan._ft_max_lvl;
			_sb.free_leaves = chan._ft_nr_of_leaves;

		} else {

			class Exception_1 { };
			throw Exception_1 { };
		}
		if (req._nr_of_blks == 0) {

			_sb.state = Superblock::NORMAL;
			req._client_req_finished = true;
		}
		_secure_sb_init(chan, progress);
		break;
	}
	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED: _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED: _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED: _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED: _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		chan._mark_req_successful(progress);
		break;

	default:
		break;
	}
}



void Superblock_control::_execute_rekey_vba(Channel  &chan,
                                            uint64_t  chan_idx,
                                            bool     &progress)
{
	Request &req { *chan._req_ptr };

	switch (chan._state) {
	case Channel::SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (_sb.state != Superblock::REKEYING) {
			chan._mark_req_failed(progress, "check superblock state");
			break;
		}
		chan._generate_vbd_req(
			*this, Virtual_block_device_request::REKEY_VBA, Channel::REKEY_VBA_AT_VBD_SUCCEEDED,
			progress, _sb.current_key.id, _sb.rekeying_vba);

		chan._state = Channel::REQ_GENERATED;
		if (VERBOSE_REKEYING) {
			log("rekey vba ", _sb.rekeying_vba, ":");
			log("  update vbd: keys ", _sb.previous_key.id, ",", _sb.current_key.id, " generations ", _sb.last_secured_generation, ",", _curr_gen);
		}
		break;

	case Channel::REKEY_VBA_AT_VBD_SUCCEEDED:
	{
		Number_of_leaves max_nr_of_leaves { 0 };

		for (Snapshot const &snap : _sb.snapshots.items) {
			if (snap.valid && max_nr_of_leaves < snap.nr_of_leaves)
				max_nr_of_leaves = snap.nr_of_leaves;
		}
		if (_sb.rekeying_vba < max_nr_of_leaves - 1) {

			_sb.rekeying_vba++;
			req._client_req_finished = false;
			_secure_sb_init(chan, progress);

			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", _curr_gen);

		} else {

			chan._prev_key_plaintext.id = _sb.previous_key.id;
			chan._state = Channel::REQ_GENERATED;
			chan.generate_req<Crypto_request>(
				Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, Crypto_request::REMOVE_KEY, 0, INVALID_REQ_TAG, chan._prev_key_plaintext.id,
				chan._prev_key_plaintext.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
				chan._generated_prim.succ);

			if (VERBOSE_REKEYING)
				log("  remove key ", (Key_id)chan._key_plaintext.id);
		}
		break;
	}
	case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_SUCCEEDED:

		_sb.previous_key = { };
		_sb.state = Superblock::NORMAL;
		req._client_req_finished = true;
		_secure_sb_init(chan, progress);

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);

		break;

	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED:  _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED:           _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED:             _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED:          _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		chan._mark_req_successful(progress);
		break;

	default:

		break;
	}
}


void Superblock_control::_secure_sb_init(Channel &chan, bool &progress)
{
	_sb.curr_snap().gen = _curr_gen;
	_init_sb_without_key_values(_sb, chan._sb_ciphertext);
	chan._key_plaintext = _sb.current_key;
	chan._generate_ta_req(Trust_anchor_request::ENCRYPT_KEY, Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED,
	                      progress, chan._sb_ciphertext.current_key.value, chan._key_plaintext.value);
}


void Superblock_control::_secure_sb_encr_curr_key_succ(Channel &chan, uint64_t chan_idx, bool &progress)
{
	if (_sb.state == Superblock::REKEYING) {
		chan._key_plaintext = _sb.previous_key;
		chan._generate_ta_req(Trust_anchor_request::ENCRYPT_KEY, Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED,
		                      progress, chan._sb_ciphertext.previous_key.value, chan._key_plaintext.value);
	} else {
		chan._generated_prim = {
			.op     = Generated_prim::SYNC,
			.succ   = false,
			.tg     = Channel::TAG_SB_CTRL_CACHE,
			.blk_nr = 0,
			.idx    = chan_idx
		};
		chan._state = Channel::SYNC_CACHE_PENDING;
		progress = true;
	}
}


void Superblock_control::_secure_sb_encr_prev_key_succ(Channel &chan, uint64_t chan_idx, bool &progress)
{
	chan._generated_prim = {
		.op     = Generated_prim::SYNC,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_CACHE,
		.blk_nr = 0,
		.idx    = chan_idx
	};
	chan._state = Channel::SYNC_CACHE_PENDING;
	progress = true;
}


void Superblock_control::_secure_sb_sync_cache_compl(Channel  &chan,
                                                     uint64_t  chan_idx,
                                                     bool     &progress)
{
	if (!chan._generated_prim.succ) {
		chan._mark_req_failed(progress, "sync cache");
		return;
	}
	chan._generated_prim = {
		.op     = Generated_prim::WRITE,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_BLK_IO_WRITE_SB,
		.blk_nr = _sb_idx,
		.idx    = chan_idx
	};
	chan._sb_ciphertext.encode_to_blk(chan._encoded_blk);
	chan._state = Channel::WRITE_SB_PENDING;
	progress = true;
}


void Superblock_control::_secure_sb_sync_blk_io_compl(Channel &chan, bool &progress)
{
		if (!chan._generated_prim.succ) {
			chan._mark_req_failed(progress, "sync block io");
			return;
		}
		Block blk { };
		chan._sb_ciphertext.encode_to_blk(blk);
		calc_sha256_4k_hash(blk, chan._hash);
		chan._generate_ta_req(Trust_anchor_request::SECURE_SUPERBLOCK, Channel::SECURE_SB_SUCCEEDED,
		                      progress, chan._sb_ciphertext.current_key.value, chan._key_plaintext.value);

		if (_sb_idx < MAX_SUPERBLOCK_INDEX)
			_sb_idx++;
		else
			_sb_idx = 0;

		chan._gen = _curr_gen;
		_curr_gen++;
}


void Superblock_control::_secure_sb_write_sb_compl(Channel  &chan,
                                                   uint64_t  chan_idx,
                                                   bool     &progress)
{
	if (!chan._generated_prim.succ) {
		chan._mark_req_failed(progress, "write superblock");
		return;
	}
	chan._generated_prim = {
		.op     = Generated_prim::SYNC,
		.succ   = false,
		.tg     = Channel::TAG_SB_CTRL_BLK_IO_SYNC,
		.blk_nr = _sb_idx,
		.idx    = chan_idx
	};
	chan._state = Channel::SYNC_BLK_IO_PENDING;
	progress = true;
}


void Superblock_control::_secure_sb_finish(Channel &chan)
{
	_sb.last_secured_generation = chan._gen;
}


void
Superblock_control::_execute_initialize_rekeying(Channel           &chan,
                                                 uint64_t   const   chan_idx,
                                                 bool              &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		chan._generate_ta_req(Trust_anchor_request::CREATE_KEY, Channel::CREATE_KEY_SUCCEEDED,
		                      progress, chan._sb_ciphertext.current_key.value, chan._key_plaintext.value);
		break;

	case Channel::CREATE_KEY_SUCCEEDED:

		if (_sb.state != Superblock::NORMAL) {
			chan._mark_req_failed(progress, "check superblock state");
			break;
		}
		_sb.state = Superblock::REKEYING;
		_sb.rekeying_vba = 0;
		_sb.previous_key = _sb.current_key;
		_sb.current_key = {
			.value = chan._key_plaintext.value,
			.id    = _sb.previous_key.id + 1
		};
		chan._key_plaintext = _sb.current_key;
		chan._state = Channel::REQ_GENERATED;
		chan.generate_req<Crypto_request>(
			Channel::ADD_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, Crypto_request::ADD_KEY, 0, INVALID_REQ_TAG, chan._key_plaintext.id,
			chan._key_plaintext.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
			chan._generated_prim.succ);

		if (VERBOSE_REKEYING) {
			log("start rekeying:");
			log("  update sb: keys ", (Key_id)_sb.previous_key.id,
			    ",", (Key_id)chan._key_plaintext.id);
		}
		break;

	case Channel::ADD_KEY_AT_CRYPTO_MODULE_SUCCEEDED:

		_secure_sb_init(chan, progress);
		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);

		break;

	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED:  _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED:           _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED:             _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED:          _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		chan._mark_req_successful(progress);
		break;

	default:

		break;
	}
}


void Superblock_control::_execute_discard_snap(Channel &chan, uint64_t chan_idx, bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::SUBMITTED:
	{
		for (Snapshot &snap : _sb.snapshots.items)
			if (snap.valid && snap.gen == req._gen && snap.keep)
				snap.keep = false;

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_secure_sb_init(chan, progress);
		break;
	}
	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED: _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED: _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED: _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED: _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		req._gen = chan._gen;
		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control::_execute_create_snap(Channel &chan, uint64_t chan_idx, bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::SUBMITTED:

		if (_sb.curr_snap().keep) {
			req._gen = _sb.curr_snap().gen;
			chan._mark_req_successful(progress);
		} else {
			_sb.curr_snap().keep = true;
			_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
			_secure_sb_init(chan, progress);
		}
		break;

	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED: _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED: _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED: _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED: _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		req._gen = chan._gen;
		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control::_execute_sync(Channel &chan, uint64_t chan_idx, bool &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_secure_sb_init(chan, progress);
		break;

	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED: _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED: _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED: _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED: _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		chan._mark_req_successful(progress);
		break;

	default:

		break;
	}
}


void Superblock_control::_execute_initialize(Channel           &chan,
                                             Superblock        &sb,
                                             Superblock_index  &sb_idx,
                                             Generation        &curr_gen,
                                             bool              &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		chan._sb_found = false;
		chan._generate_ta_req(Trust_anchor_request::GET_LAST_SB_HASH, Channel::GET_LAST_SB_HASH_SUCCEEDED,
		                      progress, chan._sb_ciphertext.current_key.value, chan._key_plaintext.value);
		break;

	case Channel::GET_LAST_SB_HASH_SUCCEEDED:

		chan._read_sb_idx = 0;
		chan._generate_blk_req(Block_io_request::READ, chan._read_sb_idx, Channel::READ_SB_COMPLETED, progress);
		break;

	case Channel::READ_SB_COMPLETED:

		chan._sb_ciphertext.decode_from_blk(chan._encoded_blk);
		if (chan._sb_ciphertext.state != Superblock::INVALID) {

			Superblock const &cipher { chan._sb_ciphertext };
			Snapshot_index const snap_index { cipher.snapshots.newest_snapshot_idx() };
			Generation const sb_generation { cipher.snapshots.items[snap_index].gen };

			if (check_sha256_4k_hash(chan._encoded_blk, chan._hash)) {
				chan._gen = sb_generation;
				chan._sb_idx     = chan._read_sb_idx;
				chan._sb_found   = true;
			}
		}
		if (chan._read_sb_idx < MAX_SUPERBLOCK_INDEX) {
			chan._read_sb_idx++;
			chan._generate_blk_req(Block_io_request::READ, chan._read_sb_idx, Channel::READ_SB_COMPLETED, progress);
		} else {
			ASSERT(chan._sb_found);
			chan._generate_blk_req(Block_io_request::READ, chan._sb_idx, Channel::READ_CURRENT_SB_COMPLETED, progress);
		}
		break;

	case Channel::READ_CURRENT_SB_COMPLETED:

		chan._sb_ciphertext.decode_from_blk(chan._encoded_blk);
		chan._generate_ta_req(Trust_anchor_request::DECRYPT_KEY, Channel::DECRYPT_CURRENT_KEY_SUCCEEDED,
		                      progress, chan._sb_ciphertext.current_key.value, chan._curr_key_plaintext.value);
		break;

	case Channel::DECRYPT_CURRENT_KEY_SUCCEEDED:

		chan._curr_key_plaintext.id = chan._sb_ciphertext.current_key.id;
		chan._state = Channel::REQ_GENERATED;
		chan.generate_req<Crypto_request>(
			Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, Crypto_request::ADD_KEY, 0, INVALID_REQ_TAG, chan._curr_key_plaintext.id,
			chan._curr_key_plaintext.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
			chan._generated_prim.succ);
		break;

	case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_SUCCEEDED:

		switch (chan._sb_ciphertext.state) {
		case Superblock::INVALID:
			class Execute_add_current_key_at_crypto_invalid_error { };
			throw Execute_add_current_key_at_crypto_invalid_error { };

			break;
		case Superblock::REKEYING:
			chan._generate_ta_req(Trust_anchor_request::DECRYPT_KEY, Channel::DECRYPT_PREVIOUS_KEY_SUCCEEDED,
			                      progress, chan._sb_ciphertext.previous_key.value, chan._prev_key_plaintext.value);
			break;
		case Superblock::NORMAL:
		case Superblock::EXTENDING_VBD:
		case Superblock::EXTENDING_FT:

			_init_sb_without_key_values(chan._sb_ciphertext, sb);

			sb.current_key.value = chan._curr_key_plaintext.value;
			sb_idx               = chan._sb_idx;
			curr_gen             = chan._gen + 1;

			sb_idx = chan._sb_idx;
			curr_gen = chan._gen + 1;

			if (sb.free_max_level < FREE_TREE_MIN_MAX_LEVEL) {
				class Execute_add_current_key_at_crypto_max_level_error { };
				throw Execute_add_current_key_at_crypto_max_level_error { };
			}

			chan._req_ptr->_sb_state = _sb.state;
			chan._mark_req_successful(progress);
			break;
		}

		break;
	case Channel::DECRYPT_PREVIOUS_KEY_SUCCEEDED:
		if (!chan._generated_prim.succ) {
			class Decrypt_previous_key_error { };
			throw Decrypt_previous_key_error { };
		}
		chan._state = Channel::REQ_GENERATED;
		chan.generate_req<Crypto_request>(
			Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, Crypto_request::ADD_KEY, 0, INVALID_REQ_TAG, chan._prev_key_plaintext.id,
			chan._prev_key_plaintext.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
			chan._generated_prim.succ);
		break;

	case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_SUCCEEDED:
		_init_sb_without_key_values(chan._sb_ciphertext, sb);

		sb.current_key.value  = chan._curr_key_plaintext.value;
		sb.previous_key.value = chan._prev_key_plaintext.value;

		sb_idx   = chan._sb_idx;
		curr_gen = chan._gen + 1;

		chan._req_ptr->_sb_state = _sb.state;
		chan._mark_req_successful(progress);
		break;
	default:
		break;
	}
}


void Superblock_control::_execute_deinitialize(Channel &chan, uint64_t chan_idx, bool &progress)
{
	switch (chan._state) {
	case Channel::SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_secure_sb_init(chan, progress);
		break;

	case Channel::ENCRYPT_CURRENT_KEY_SUCCEEDED: _secure_sb_encr_curr_key_succ(chan, chan_idx, progress); break;
	case Channel::ENCRYPT_PREVIOUS_KEY_SUCCEEDED: _secure_sb_encr_prev_key_succ(chan, chan_idx, progress); break;
	case Channel::SYNC_CACHE_COMPLETED: _secure_sb_sync_cache_compl(chan, chan_idx, progress); break;
	case Channel::WRITE_SB_COMPLETED: _secure_sb_write_sb_compl(chan, chan_idx, progress); break;
	case Channel::SYNC_BLK_IO_COMPLETED: _secure_sb_sync_blk_io_compl(chan, progress); break;
	case Channel::SECURE_SB_SUCCEEDED:

		_secure_sb_finish(chan);
		chan._curr_key_plaintext.id = _sb.current_key.id;
		chan.generate_req<Crypto_request>(
			Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, Crypto_request::REMOVE_KEY, 0, INVALID_REQ_TAG, chan._curr_key_plaintext.id,
			chan._curr_key_plaintext.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
			chan._generated_prim.succ);
		break;

	case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING) {
			chan._prev_key_plaintext.id = _sb.previous_key.id;
			chan._state = Channel::REQ_GENERATED;
			chan.generate_req<Crypto_request>(
				Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, Crypto_request::REMOVE_KEY, 0, INVALID_REQ_TAG, chan._prev_key_plaintext.id,
				chan._prev_key_plaintext.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
				chan._generated_prim.succ);
		} else
			chan._mark_req_successful(progress);
		break;

	case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_SUCCEEDED:

		_sb.state = Superblock::INVALID;
		chan._mark_req_successful(progress);
		break;
	default:
		break;
	}
}


void Superblock_control_channel::_generate_blk_req(Block_io_request::Type type, Physical_block_address pba, State complete_state,
                                                   bool &progress)
{
	_state = REQ_GENERATED;
	generate_req<Block_io_request>(
		complete_state, progress, type, 0, 0, 0, pba, 0, 1, _encoded_blk, _hash, _generated_prim.succ);
}


bool Superblock_control::_peek_generated_request(uint8_t *buf_ptr,
                                                 size_t   buf_size)
{
	for (Module_channel_id id = 0; id < NUM_CHANNELS; id++) {

		Channel &chan { _channels[id] };
		if (!chan.req_valid())
			continue;

		switch (chan._state) {
		case Channel::SYNC_BLK_IO_PENDING:
		case Channel::SYNC_CACHE_PENDING:

			ASSERT(sizeof(Block_io_request) <= buf_size);
			construct_at<Block_io_request>(
				buf_ptr, SUPERBLOCK_CONTROL, id,
				Block_io_request::SYNC, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, chan._encoded_blk, chan._hash, chan._generated_prim.succ);

			return true;

		case Channel::WRITE_SB_PENDING:

			chan._sb_ciphertext.encode_to_blk(chan._encoded_blk);

			ASSERT(sizeof(Block_io_request) <= buf_size);
			construct_at<Block_io_request>(
				buf_ptr, SUPERBLOCK_CONTROL, id,
				Block_io_request::WRITE, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, chan._encoded_blk,
				chan._hash, chan._generated_prim.succ);

			return true;

		default: break;
		}
	}
	return false;
}


void Superblock_control::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NUM_CHANNELS) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._state) {
	case Channel::SYNC_BLK_IO_PENDING: chan._state = Channel::SYNC_BLK_IO_IN_PROGRESS; break;
	case Channel::SYNC_CACHE_PENDING: chan._state = Channel::SYNC_CACHE_IN_PROGRESS; break;
	case Channel::WRITE_SB_PENDING: chan._state = Channel::WRITE_SB_IN_PROGRESS; break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Superblock_control::execute(bool &progress)
{
	for (unsigned idx = 0; idx < NUM_CHANNELS; idx++) {

		Channel &chan = _channels[idx];
		if (!chan.req_valid())
			continue;

		switch (chan._req_ptr->_type) {
		case Request::READ_VBA: chan._access_vba(*this, Virtual_block_device_request::READ_VBA, progress); break;
		case Request::WRITE_VBA: chan._access_vba(*this, Virtual_block_device_request::WRITE_VBA, progress); break;
		case Request::SYNC: _execute_sync(chan, idx, progress); break;
		case Request::INITIALIZE_REKEYING: _execute_initialize_rekeying(chan, idx, progress); break;
		case Request::REKEY_VBA: _execute_rekey_vba(chan, idx, progress); break;
		case Request::VBD_EXTENSION_STEP: _execute_tree_ext_step(chan, idx, Superblock::EXTENDING_VBD, VERBOSE_VBD_EXTENSION, "vbd", progress); break;
		case Request::FT_EXTENSION_STEP: _execute_tree_ext_step(chan, idx, Superblock::EXTENDING_FT, VERBOSE_FT_EXTENSION, "ft", progress); break;
		case Request::CREATE_SNAPSHOT: _execute_create_snap(chan, idx, progress); break;
		case Request::DISCARD_SNAPSHOT: _execute_discard_snap(chan, idx, progress); break;
		case Request::INITIALIZE: _execute_initialize(chan, _sb, _sb_idx, _curr_gen, progress); break;
		case Request::DEINITIALIZE: _execute_deinitialize (chan, idx, progress); break;
		}
	}
}


void Superblock_control::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NUM_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	ASSERT(chan.req_valid());
	switch (mod_req.dst_module_id()) {
	case BLOCK_IO:
	{
		switch (chan._state) {
		case Channel::SYNC_BLK_IO_IN_PROGRESS: chan._state = Channel::SYNC_BLK_IO_COMPLETED; break;
		case Channel::SYNC_CACHE_IN_PROGRESS: chan._state = Channel::SYNC_CACHE_COMPLETED; break;
		case Channel::WRITE_SB_IN_PROGRESS: chan._state = Channel::WRITE_SB_COMPLETED; break;
		default:
			class Exception_7 { };
			throw Exception_7 { };
		}
		break;
	}
	default:
		class Exception_8 { };
		throw Exception_8 { };
	}
}


void Superblock_control_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request *>(&req);
	_state = SUBMITTED;
}
