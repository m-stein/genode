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
	_state = REQ_COMPLETE;
	progress = true;
}


void Superblock_control_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
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
	if (!_gen_req_success) {
		error("request_pool: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
	} else {
		if (_state == SECURE_SB)
			_secure_sb_state = (Secure_sb_state)state_uint;
		else
			_state = (State)state_uint;
	}
}


void Superblock_control_channel::
_generate_vbd_req(Superblock_control &mod, Virtual_block_device_request::Type type,
                  State_uint complete_state, bool &progress, Key_id key_id, Virtual_block_address vba = INVALID_VBA)
{
	if (_state == SECURE_SB)
		_secure_sb_state = SECURE_SB_REQ_GENERATED;
	else
		_state = REQ_GENERATED;
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
		_pba, _gen_req_success, _nr_of_leaves, req._nr_of_blks);
}

void Superblock_control_channel::_generate_ta_req(Trust_anchor_request::Type type, State_uint complete_state,
                                                  bool &progress, Key_value &key_ciphertext, Key_value &key_plaintext)
{
	if (_state == SECURE_SB)
		_secure_sb_state = SECURE_SB_REQ_GENERATED;
	else
		_state = REQ_GENERATED;
	generate_req<Trust_anchor_request>(
		complete_state, progress, type, key_plaintext, key_ciphertext, _hash, Passphrase { }, _gen_req_success);
}


void Superblock_control_channel::_access_vba(Superblock_control &mod, Virtual_block_device_request::Type type, bool &progress)
{
	Request &req { *_req_ptr };
	Generation &curr_gen { mod._curr_gen };
	Superblock &sb { mod._sb };
	switch (_state) {
	case REQ_SUBMITTED:
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


void Superblock_control::_execute_tree_ext_step(Channel &chan,
                                                Superblock::State tree_ext_sb_state,
                                                bool tree_ext_verbose,
                                                String<4> tree_name,
                                                bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:
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

			_start_secure_sb(chan, progress);
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
				chan.generate_req<Ft_resizing_request>(
					Channel::TREE_EXT_STEP_IN_TREE_SUCCEEDED, progress, Ft_resizing_request::FT_EXTENSION_STEP,
					_curr_gen, chan._ft_root, _sb.free_max_level, _sb.free_leaves,
					_sb.free_degree, _sb.meta_number, _sb.meta_gen, _sb.meta_hash,
					_sb.meta_max_level, _sb.meta_degree, _sb.meta_leaves, chan._pba,
					chan._req_ptr->_nr_of_blks, chan._gen_req_success);
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

		if (tree_name == "vbd")
			_sb.curr_snap_idx = _sb.snapshots.newest_snapshot_idx();
		else if (tree_name == "ft") {
			_sb.free_gen = chan._ft_root.gen;
			_sb.free_number = chan._ft_root.pba;
			_sb.free_hash = chan._ft_root.hash;
		} else
			ASSERT_NEVER_REACHED;

		if (!req._nr_of_blks) {
			_sb.state = Superblock::NORMAL;
			req._client_req_finished = true;
		}
		_start_secure_sb(chan, progress);
		break;
	}
	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		chan._mark_req_successful(progress);
		break;

	default:
		break;
	}
}



void Superblock_control::_execute_rekey_vba(Channel  &chan,
                                            bool     &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

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
			_start_secure_sb(chan, progress);

			if (VERBOSE_REKEYING)
				log("  secure sb: gen ", _curr_gen);

		} else {

			chan._state = Channel::REQ_GENERATED;
			chan.generate_req<Crypto_request>(
				Channel::REMOVE_PREV_KEY_SUCCEEDED, progress, Crypto_request::REMOVE_KEY, 0, INVALID_REQ_TAG, _sb.previous_key.id,
				_sb.previous_key.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
				chan._gen_req_success);

			if (VERBOSE_REKEYING)
				log("  remove key ", _sb.previous_key.id);
		}
		break;
	}
	case Channel::REMOVE_PREV_KEY_SUCCEEDED:

		_sb.previous_key = { };
		_sb.state = Superblock::NORMAL;
		req._client_req_finished = true;
		_start_secure_sb(chan, progress);
		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);
		break;

	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		chan._mark_req_successful(progress);
		break;

	default:

		break;
	}
}


void Superblock_control::_start_secure_sb(Channel &chan, bool &progress)
{
	chan._state = Channel::SECURE_SB;
	chan._secure_sb_state = Channel::STARTED;
	progress = true;
}


void Superblock_control::_secure_sb(Channel &chan, bool &progress)
{
	switch (chan._secure_sb_state) {
	case Channel::STARTED:

		_sb.curr_snap().gen = _curr_gen;
		chan._sb_ciphertext.copy_all_but_key_values_from(_sb);
		chan._generate_ta_req(
			Trust_anchor_request::ENCRYPT_KEY, Channel::ENCRYPT_CURR_KEY_SUCCEEDED, progress,
			chan._sb_ciphertext.current_key.value, _sb.current_key.value);
		break;

	case Channel::ENCRYPT_CURR_KEY_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING)
			chan._generate_ta_req(
				Trust_anchor_request::ENCRYPT_KEY, Channel::ENCRYPT_PREV_KEY_SUCCEEDED, progress,
				chan._sb_ciphertext.previous_key.value, _sb.previous_key.value);
		else
			chan._generate_blk_req(Block_io_request::SYNC, 0, Channel::SYNC_CACHE_SUCCEEDED, progress);
		break;

	case Channel::ENCRYPT_PREV_KEY_SUCCEEDED:

		chan._generate_blk_req(Block_io_request::SYNC, 0, Channel::SYNC_CACHE_SUCCEEDED, progress);
		break;

	case Channel::SYNC_CACHE_SUCCEEDED:

		chan._sb_ciphertext.encode_to_blk(chan._encoded_blk);
		chan._generate_blk_req(Block_io_request::WRITE, _sb_idx, Channel::WRITE_SB_SUCCEEDED, progress);
		break;

	case Channel::WRITE_SB_SUCCEEDED:

		chan._generate_blk_req(Block_io_request::SYNC, _sb_idx, Channel::SYNC_BLK_IO_SUCCEEDED, progress);
		break;

	case Channel::SYNC_BLK_IO_SUCCEEDED:
	{
		Block blk { };
		chan._sb_ciphertext.encode_to_blk(blk);
		calc_sha256_4k_hash(blk, chan._hash);
		chan._generate_ta_req(Trust_anchor_request::SECURE_SUPERBLOCK, Channel::SECURE_SB_AT_TA_SUCCEEDED,
		                      progress, chan._sb_ciphertext.current_key.value, _sb.current_key.value);

		if (_sb_idx < MAX_SUPERBLOCK_INDEX)
			_sb_idx++;
		else
			_sb_idx = 0;

		chan._gen = _curr_gen;
		_curr_gen++;
		break;
	}
	case Channel::SECURE_SB_AT_TA_SUCCEEDED:

		_sb.last_secured_generation = chan._gen;
		chan._state = Channel::SECURE_SB_DONE;
		break;

	default: break;
	}
}


void Superblock_control::_execute_initialize_rekeying(Channel &chan, bool &progress)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		if (_sb.state != Superblock::NORMAL) {
			chan._mark_req_failed(progress, "check superblock state");
			break;
		}
		_sb.state = Superblock::REKEYING;
		_sb.rekeying_vba = 0;
		_sb.previous_key = _sb.current_key;
		_sb.current_key.id++;
		chan._generate_req<Trust_anchor::Create_key>(Channel::CREATE_KEY_SUCCEEDED, progress, _sb.current_key.value);
		break;

	case Channel::CREATE_KEY_SUCCEEDED:

		chan._generate_req<Crypto::Add_key>(Channel::ADD_KEY_AT_CRYPTO_MODULE_SUCCEEDED, progress, _sb.current_key);
		if (VERBOSE_REKEYING)
			log("start rekeying:\n  update sb: keys ", _sb.previous_key.id, ",", _sb.current_key.id);
		break;

	case Channel::ADD_KEY_AT_CRYPTO_MODULE_SUCCEEDED:

		if (VERBOSE_REKEYING)
			log("  secure sb: gen ", _curr_gen);
		_start_secure_sb(chan, progress);
		break;

	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control::_execute_discard_snap(Channel &chan, bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		for (Snapshot &snap : _sb.snapshots.items)
			if (snap.valid && snap.gen == req._gen && snap.keep)
				snap.keep = false;

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_start_secure_sb(chan, progress);
		break;

	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		req._gen = chan._gen;
		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control::_execute_create_snap(Channel &chan, bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		if (_sb.curr_snap().keep) {
			req._gen = _sb.curr_snap().gen;
			chan._mark_req_successful(progress);
		} else {
			_sb.curr_snap().keep = true;
			_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
			_start_secure_sb(chan, progress);
		}
		break;

	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		req._gen = chan._gen;
		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control::_execute_sync(Channel &chan, bool &progress)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_start_secure_sb(chan, progress);
		break;

	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control_request::print(Output &out) const
{
	Genode::print(out, type_to_string(_type));
	switch (_type) {
	case REKEY_VBA:
	case READ_VBA:
	case WRITE_VBA:
		Genode::print(out, " ", _vba);
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
	case Channel::REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		chan._sb_found = false;
		chan._generate_ta_req(
			Trust_anchor_request::GET_LAST_SB_HASH, Channel::GET_LAST_SB_HASH_SUCCEEDED, progress,
			chan._sb_ciphertext.current_key.value, sb.current_key.value);
		break;

	case Channel::GET_LAST_SB_HASH_SUCCEEDED:

		chan._read_sb_idx = 0;
		chan._generate_blk_req(Block_io_request::READ, chan._read_sb_idx, Channel::READ_SB_SUCCEEDED, progress);
		break;

	case Channel::READ_SB_SUCCEEDED:

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
			chan._generate_blk_req(Block_io_request::READ, chan._read_sb_idx, Channel::READ_SB_SUCCEEDED, progress);
		} else {
			ASSERT(chan._sb_found);
			chan._generate_blk_req(Block_io_request::READ, chan._sb_idx, Channel::READ_CURR_SB_SUCCEEDED, progress);
		}
		break;

	case Channel::READ_CURR_SB_SUCCEEDED:

		chan._sb_ciphertext.decode_from_blk(chan._encoded_blk);
		chan._generate_ta_req(
			Trust_anchor_request::DECRYPT_KEY, Channel::DECRYPT_CURR_KEY_SUCCEEDED, progress,
			chan._sb_ciphertext.current_key.value, sb.current_key.value);
		break;

	case Channel::DECRYPT_CURR_KEY_SUCCEEDED:

		sb.current_key.id = chan._sb_ciphertext.current_key.id;
		chan._generate_req<Crypto::Add_key>(Channel::ADD_CURR_KEY_SUCCEEDED, progress, sb.current_key);
		break;

	case Channel::ADD_CURR_KEY_SUCCEEDED:

		ASSERT(chan._sb_ciphertext.state != Superblock::INVALID);
		if (chan._sb_ciphertext.state == Superblock::REKEYING) {
			chan._generate_ta_req(
				Trust_anchor_request::DECRYPT_KEY, Channel::DECRYPT_PREV_KEY_SUCCEEDED, progress,
				chan._sb_ciphertext.previous_key.value, sb.previous_key.value);
		} else {
			sb.copy_all_but_key_values_from(chan._sb_ciphertext);
			sb_idx = chan._sb_idx;
			curr_gen = chan._gen + 1;
			ASSERT(sb.free_max_level >= FREE_TREE_MIN_MAX_LEVEL);
			chan._req_ptr->_sb_state = _sb.state;
			chan._mark_req_successful(progress);
		}
		break;

	case Channel::DECRYPT_PREV_KEY_SUCCEEDED:

		chan._state = Channel::REQ_GENERATED;
		chan.generate_req<Crypto_request>(
			Channel::ADD_PREV_KEY_SUCCEEDED, progress, Crypto_request::ADD_KEY, 0, INVALID_REQ_TAG,
			chan._sb_ciphertext.previous_key.id, sb.previous_key.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
			chan._gen_req_success);
		break;

	case Channel::ADD_PREV_KEY_SUCCEEDED:

		sb.copy_all_but_key_values_from(chan._sb_ciphertext);
		sb_idx   = chan._sb_idx;
		curr_gen = chan._gen + 1;
		chan._req_ptr->_sb_state = _sb.state;
		chan._mark_req_successful(progress);
		break;

	default: break;
	}
}


void Superblock_control::_execute_deinitialize(Channel &chan, bool &progress)
{
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:

		_sb.snapshots.discard_disposable_snapshots(_sb.last_secured_generation, _curr_gen);
		_sb.last_secured_generation = _curr_gen;
		_start_secure_sb(chan, progress);
		break;

	case Channel::SECURE_SB: _secure_sb(chan, progress); break;
	case Channel::SECURE_SB_DONE:

		chan.generate_req<Crypto_request>(
			Channel::REMOVE_CURR_KEY_SUCCEEDED, progress, Crypto_request::REMOVE_KEY, 0, INVALID_REQ_TAG, _sb.current_key.id,
			_sb.current_key.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
			chan._gen_req_success);
		break;

	case Channel::REMOVE_CURR_KEY_SUCCEEDED:

		if (_sb.state == Superblock::REKEYING) {
			chan._state = Channel::REQ_GENERATED;
			chan.generate_req<Crypto_request>(
				Channel::REMOVE_PREV_KEY_SUCCEEDED, progress, Crypto_request::REMOVE_KEY, 0, INVALID_REQ_TAG, _sb.previous_key.id,
				_sb.previous_key.value, INVALID_PBA, INVALID_VBA, chan._encoded_blk, chan._encoded_blk,
				chan._gen_req_success);
		} else
			chan._mark_req_successful(progress);
		break;

	case Channel::REMOVE_PREV_KEY_SUCCEEDED:

		_sb.state = Superblock::INVALID;
		chan._mark_req_successful(progress);
		break;
	default:
		break;
	}
}


void Superblock_control_channel::_generate_blk_req(Block_io_request::Type type, Physical_block_address pba, State_uint complete_state,
                                                   bool &progress)
{
	if (_state == SECURE_SB)
		_secure_sb_state = SECURE_SB_REQ_GENERATED;
	else
		_state = REQ_GENERATED;
	generate_req<Block_io_request>(
		complete_state, progress, type, 0, 0, 0, pba, 0, 1, _encoded_blk, _hash, _gen_req_success);
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
		case Request::SYNC: _execute_sync(chan, progress); break;
		case Request::INITIALIZE_REKEYING: _execute_initialize_rekeying(chan, progress); break;
		case Request::REKEY_VBA: _execute_rekey_vba(chan, progress); break;
		case Request::VBD_EXTENSION_STEP: _execute_tree_ext_step(chan, Superblock::EXTENDING_VBD, VERBOSE_VBD_EXTENSION, "vbd", progress); break;
		case Request::FT_EXTENSION_STEP: _execute_tree_ext_step(chan, Superblock::EXTENDING_FT, VERBOSE_FT_EXTENSION, "ft", progress); break;
		case Request::CREATE_SNAPSHOT: _execute_create_snap(chan, progress); break;
		case Request::DISCARD_SNAPSHOT: _execute_discard_snap(chan, progress); break;
		case Request::INITIALIZE: _execute_initialize(chan, _sb, _sb_idx, _curr_gen, progress); break;
		case Request::DEINITIALIZE: _execute_deinitialize (chan, progress); break;
		}
	}
}


void Superblock_control_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request *>(&req);
	_state = REQ_SUBMITTED;
}
