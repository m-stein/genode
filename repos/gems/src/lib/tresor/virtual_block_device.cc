/*
 * \brief  Module for accessing and managing trees of the virtual block device
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>
#include <util/misc_math.h>

/* tresor includes */
#include <tresor/virtual_block_device.h>
#include <tresor/sha256_4k_hash.h>
#include <tresor/block_io.h>
#include <tresor/crypto.h>

using namespace Tresor;

Virtual_block_device_request::
Virtual_block_device_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                             Request_offset client_req_offset, Request_tag client_req_tag,
                             Generation last_secured_generation, Free_tree_root &ft, Meta_tree_root &mt,
                             Tree_degree vbd_degree, Virtual_block_address vbd_highest_vba, bool rekeying,
                             Virtual_block_address vba, Snapshot_index curr_snap_idx, Snapshots &snapshots,
                             Tree_degree snap_degr, Key_id prev_key_id, Key_id curr_key_id,
                             Generation curr_gen, Physical_block_address &pba, bool &success,
                             Number_of_leaves &nr_of_leaves, Number_of_blocks &nr_of_pbas)
:
	Module_request { src_module_id, src_chan_id, VIRTUAL_BLOCK_DEVICE }, _type { type },
	_vba { vba }, _snapshots { snapshots }, _curr_snap_idx { curr_snap_idx },
	_snap_degr { snap_degr }, _curr_gen { curr_gen }, _curr_key_id { curr_key_id },
	_prev_key_id { prev_key_id }, _ft { ft }, _mt { mt }, _vbd_degree { vbd_degree },
	_vbd_highest_vba { vbd_highest_vba }, _rekeying { rekeying }, _client_req_offset { client_req_offset },
	_client_req_tag { client_req_tag }, _last_secured_generation { last_secured_generation }, _pba { pba },
	_nr_of_pbas { nr_of_pbas }, _nr_of_leaves { nr_of_leaves }, _success { success }
{ }


char const *Virtual_block_device_request::type_to_string(Type op)
{
	switch (op) {
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case REKEY_VBA: return "rekey_vba";
	case VBD_EXTENSION_STEP: return "vbd_extension_step";
	}
	ASSERT_NEVER_REACHED;
}


void Virtual_block_device_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_gen_req_success) {
		error("request_pool: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Virtual_block_device_channel::_generate_write_node_req(bool &progress)
{
	State state { _lvl < snap().max_level ? WRITE_INNER_NODE_SUCCEEDED : WRITE_ROOT_NODE_SUCCEEDED };
	_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
	_generate_req<Block_io::Write>(state, progress, _new_pbas.pbas[_lvl], _encoded_blk);
}


void Virtual_block_device_channel::_read_vba(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_snap_idx = req._curr_snap_idx;
		_vba = req._vba;
		_lvl = snap().max_level;
		_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
		if (VERBOSE_READ_VBA)
			log("  load branch:\n    ", Branch_lvl_prefix("root: "), snap());
		break;

	case READ_BLK_SUCCEEDED:
	{
		_check_and_decode_read_t1_blk(progress);
		Tree_node_index node_idx { t1_node_idx_for_vba(req._vba, _lvl, req._snap_degr) };
		Type_1_node &node { _t1_blks.items[_lvl].nodes[node_idx] };
		if (VERBOSE_READ_VBA)
			log("    ", Branch_lvl_prefix("lvl ", _lvl, " node ", node_idx, ": "), node);

		if (_lvl > 1) {
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _encoded_blk);
			_lvl--;
		} else
			_generate_req<Block_io::Read_client_data>(
				READ_CLIENT_DATA_FROM_LEAF_NODE_SUCCEEDED, progress, node.pba, _vba, req._curr_key_id,
				req._client_req_tag, req._client_req_offset, _data_blk);
		break;
	}
	case READ_CLIENT_DATA_FROM_LEAF_NODE_SUCCEEDED: _mark_req_successful(progress); break;
	default: break;
	}
}


void Virtual_block_device_channel::_update_nodes_of_branch_of_written_vba()
{
	Request &req { *_req_ptr };
	for (Tree_level_index lvl = 0; lvl <= snap().max_level; lvl++) {

		if (lvl == 0) {

			Tree_node_index node_idx { t1_node_idx_for_vba(_vba, lvl + 1, req._snap_degr) };
			Type_1_node &node { _t1_blks.items[lvl + 1].nodes[node_idx] };
			node = Type_1_node { _new_pbas.pbas[lvl], req._curr_gen, _hash };
			if (VERBOSE_WRITE_VBA)
				log("    ", Branch_lvl_prefix("lvl ", lvl + 1, " node ", node_idx, ": "), node);

		} else if (lvl < snap().max_level) {

			Tree_node_index node_idx { t1_node_idx_for_vba(_vba, lvl + 1, req._snap_degr) };
			Type_1_node &node { _t1_blks.items[lvl + 1].nodes[node_idx] };
			node.pba = _new_pbas.pbas[lvl];
			node.gen = req._curr_gen;
			Block blk { };
			_t1_blks.items[lvl].encode_to_blk(blk);
			calc_sha256_4k_hash(blk, node.hash);
			if (VERBOSE_WRITE_VBA)
				log("    ", Branch_lvl_prefix("lvl ", lvl + 1, " node ", node_idx, ": "), node);

		} else {

			snap().pba = _new_pbas.pbas[lvl];
			snap().gen = req._curr_gen;
			Block blk { };
			_t1_blks.items[lvl].encode_to_blk(blk);
			calc_sha256_4k_hash(blk, snap().hash);
			if (VERBOSE_WRITE_VBA)
				log("    ", Branch_lvl_prefix("root: "), snap());
		}
	}
}


void Virtual_block_device_channel::_check_and_decode_read_t1_blk(bool &progress)
{
	Hash &hash { _lvl < snap().max_level ?
		_t1_blks.items[_lvl + 1].nodes[t1_node_idx_for_vba(_vba, _lvl + 1, _req_ptr->_snap_degr)].hash :
		snap().hash };

	if (!check_sha256_4k_hash(_encoded_blk, hash))
		_mark_req_failed(progress, "check read t1 node hash");

	_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);
}


void Virtual_block_device_channel::_set_new_pbas_and_nr_of_blks_for_alloc()
{
	Request &req { *_req_ptr };
	_nr_of_blks = 0;
	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {
		if (lvl > snap().max_level)
			_new_pbas.pbas[lvl] = 0;
		else if (lvl == snap().max_level) {
			if (snap().gen < req._curr_gen) {
				_nr_of_blks++;
				_new_pbas.pbas[lvl] = 0;
			} else if (snap().gen == req._curr_gen)
				_new_pbas.pbas[lvl] = snap().pba;
			else
				ASSERT_NEVER_REACHED;
		} else {
			Tree_node_index node_idx { t1_node_idx_for_vba(_vba, lvl + 1, req._snap_degr) };
			Type_1_node const &node { _t1_blks.items[lvl + 1].nodes[node_idx] };
			if (node.gen < req._curr_gen) {
				if (lvl == 0 && node.gen == INVALID_GENERATION)
					_new_pbas.pbas[lvl] = node.pba;
				else {
					_nr_of_blks++;
					_new_pbas.pbas[lvl] = 0;
				}
			} else if (node.gen == req._curr_gen)
				_new_pbas.pbas[lvl] = node.pba;
			else
				ASSERT_NEVER_REACHED;
		}
	}
}


void Virtual_block_device_channel::_generate_ft_alloc_req_for_write_vba(bool &progress)
{
	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {
		if (lvl > snap().max_level)
			_t1_node_walk.nodes[lvl] = Type_1_node { };
		else if (lvl == snap().max_level)
			_t1_node_walk.nodes[lvl] = Type_1_node { snap().pba, snap().gen, snap().hash };
		else
			_t1_node_walk.nodes[lvl] = _t1_blks.items[lvl + 1].nodes[t1_node_idx_for_vba(_vba, lvl + 1, _req_ptr->_snap_degr)];
	}
	_free_gen = _req_ptr->_curr_gen;
	_generate_ft_req(ALLOC_PBAS_AT_LEAF_LVL_SUCCEEDED, progress, Free_tree_request::ALLOC_FOR_NON_RKG);
}


void Virtual_block_device_channel::_write_vba(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_snap_idx = req._curr_snap_idx;
		_vba = req._vba;
		_lvl = snap().max_level;
		_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, snap().pba, _encoded_blk);
		if (VERBOSE_WRITE_VBA)
			log("  load branch:\n    ", Branch_lvl_prefix("root: "), snap());
		break;

	case READ_BLK_SUCCEEDED:

		_check_and_decode_read_t1_blk(progress);
		if (VERBOSE_WRITE_VBA) {
			Tree_node_index node_idx { t1_node_idx_for_vba(_vba, _lvl, req._snap_degr) };
			Type_1_node &node { _t1_blks.items[_lvl].nodes[node_idx] };
			log("    ", Branch_lvl_prefix("lvl ", _lvl, " node ", node_idx, ": "), node);
		}
		if (_lvl > 1) {
			Physical_block_address pba { _t1_blks.items[_lvl].nodes[t1_node_idx_for_vba(_vba, _lvl, req._snap_degr)].pba };
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, pba, _encoded_blk);
		} else {
			_set_new_pbas_and_nr_of_blks_for_alloc();
			if (_nr_of_blks)
				_generate_ft_alloc_req_for_write_vba(progress);
			else
				_generate_req<Block_io::Write_client_data>(
					WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[0], _vba,
					req._curr_key_id, req._client_req_tag, req._client_req_offset, _data_blk, _hash);
		}
		_lvl--;
		break;

	case ALLOC_PBAS_AT_LEAF_LVL_SUCCEEDED:

		if (VERBOSE_WRITE_VBA)
			log("  alloc pba", _nr_of_blks > 1 ? "s" : "", ": ", Pba_allocation(_t1_node_walk, _new_pbas));

		_generate_req<Block_io::Write_client_data>(
			WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[0], _vba, req._curr_key_id,
			req._client_req_tag, req._client_req_offset, _data_blk, _hash);

		break;

	case WRITE_BLK_SUCCEEDED:

		if (_lvl == 0)
			_update_nodes_of_branch_of_written_vba();

		if (_lvl < snap().max_level) {
			_lvl++;
			_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
			_generate_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, _new_pbas.pbas[_lvl], _encoded_blk);
		} else
		 	_mark_req_successful(progress);
		break;

	default: break;
	}
}


void Virtual_block_device_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Virtual_block_device_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


char const *Virtual_block_device::_state_to_step_label(Channel::State state)
{
	switch (state) {
	case Channel::ALLOC_PBAS_AT_LEAF_LVL_SUCCEEDED: return "alloc pbas at leaf lvl";
	case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED: return "alloc pbas at lowest inner lvl";
	case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_SUCCEEDED: return "alloc pbas at higher inner lvl";
	default: break;
	}
	return "?";
}


bool Virtual_block_device_channel::_find_next_snap_to_rekey_vba_at(Snapshot_index &next_snap_idx) const
{
	bool next_snap_idx_valid { false };
	Request const &req { *_req_ptr };
	Snapshot const &old_snap { req._snapshots.items[_snap_idx] };

	for (Snapshot_index snap_idx { 0 };
	     snap_idx < MAX_NR_OF_SNAPSHOTS;
	     snap_idx++) {

		Snapshot &snap { (req._snapshots).items[snap_idx] };
		if (snap.valid && snap.contains_vba(req._vba)) {

			if (next_snap_idx_valid) {

				Snapshot const &next_snap { (req._snapshots).items[next_snap_idx] };
				if (snap.gen > next_snap.gen &&
				    snap.gen < old_snap.gen)
					next_snap_idx = snap_idx;

			} else {

				if (snap.gen < old_snap.gen) {

					next_snap_idx = snap_idx;
					next_snap_idx_valid = true;
				}
			}
		}
	}
	return next_snap_idx_valid;
}


void Virtual_block_device::
_set_args_for_alloc_of_new_pbas_for_rekeying(Channel          &chan,
                                             Tree_level_index  min_lvl)
{
	bool const for_curr_gen_blks { chan._first_snapshot };
	Generation const curr_gen { chan._req_ptr->_curr_gen };
	Snapshot const &snap { (chan._req_ptr->_snapshots).items[chan._snap_idx] };
	Tree_degree const snap_degree { chan._req_ptr->_snap_degr };
	Virtual_block_address const vba { chan._req_ptr->_vba };
	Channel::Type_1_node_blocks const &t1_blks { chan._t1_blks };
	Type_1_node_walk &t1_walk { chan._t1_node_walk };
	Tree_walk_pbas &new_pbas { chan._new_pbas };

	if (min_lvl > snap.max_level) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	chan._nr_of_blks = 0;

	if (for_curr_gen_blks)
		chan._free_gen = curr_gen;
	else
		chan._free_gen = snap.gen + 1;

	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {

		if (lvl > snap.max_level) {

			t1_walk.nodes[lvl] = { };
			new_pbas.pbas[lvl] = 0;

		} else if (lvl == snap.max_level) {

			chan._nr_of_blks++;
			new_pbas.pbas[lvl] = 0;
			t1_walk.nodes[lvl] = { snap.pba, snap.gen, snap.hash };

		} else if (lvl >= min_lvl) {

			chan._nr_of_blks++;
			new_pbas.pbas[lvl] = 0;
			Tree_node_index const child_idx {
				t1_node_idx_for_vba(vba, lvl + 1, snap_degree) };

			t1_walk.nodes[lvl] = t1_blks.items[lvl + 1].nodes[child_idx];

		} else {

			Tree_node_index const child_idx {
				t1_node_idx_for_vba(vba, lvl + 1, snap_degree) };

			Type_1_node const &child { t1_blks.items[lvl + 1].nodes[child_idx] };
			t1_walk.nodes[lvl] = { new_pbas.pbas[lvl], child.gen, child.hash};
		}
	}
}


void Virtual_block_device_channel::_log_rekeying_pba_alloc() const
{
	if (VERBOSE_REKEYING)
		log("      alloc pba", _nr_of_blks > 1 ? "s" : "", ": ", Pba_allocation { _t1_node_walk, _new_pbas });
}


Free_tree_request::Type Virtual_block_device_channel::_ft_rkg_alloc_type() const
{
	return _first_snapshot ?
		Free_tree_request::ALLOC_FOR_RKG_CURR_GEN_BLKS :
		Free_tree_request::ALLOC_FOR_RKG_OLD_GEN_BLKS;
}


void Virtual_block_device::_execute_rekey_vba(Channel  &chan,
                                              bool     &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::State::SUBMITTED:
	{
		chan._vba = req._vba;
		Snapshot_index first_snap_idx { 0 };
		bool first_snap_idx_found { false };
		for (Snapshot_index snap_idx { 0 };
		     snap_idx < MAX_NR_OF_SNAPSHOTS;
		     snap_idx++) {

			Snapshot const &snap { (req._snapshots).items[snap_idx] };
			Snapshot const &first_snap { (req._snapshots).items[first_snap_idx] };
			if (snap.valid &&
			    (!first_snap_idx_found || snap.gen > first_snap.gen)) {

				first_snap_idx = snap_idx;
				first_snap_idx_found = true;
			}
		}
		if (!first_snap_idx_found) {

			class Exception_1 { };
			throw Exception_1 { };
		}
		chan._snap_idx = first_snap_idx;
		chan._first_snapshot = true;

		Snapshot const &snap { (req._snapshots).items[chan._snap_idx] };
		chan._lvl = snap.max_level;
		chan._t1_blks_old_pbas.items[chan._lvl] = snap.pba;

		if (VERBOSE_REKEYING) {
			log("    snapshot ", chan._snap_idx, ":");
			log("      load branch:");
			log("        ", Branch_lvl_prefix("root: "), snap);
		}
		chan._generate_req<Block_io::Read>(Channel::READ_ROOT_NODE_SUCCEEDED, progress, snap.pba, chan._encoded_blk);
		break;
	}
	case Channel::READ_ROOT_NODE_SUCCEEDED:
	case Channel::READ_INNER_NODE_SUCCEEDED:
	{
		chan._t1_blks.items[chan._lvl].decode_from_blk(chan._encoded_blk);
		Snapshot const &snap { (req._snapshots).items[chan._snap_idx] };
		if (chan._lvl == snap.max_level) {

			if (!check_sha256_4k_hash(chan._encoded_blk, snap.hash)) {

				chan._mark_req_failed(progress, "check root node hash");
				break;
			}

		} else {

			Tree_level_index const parent_lvl { chan._lvl + 1 };
			Tree_node_index  const child_idx  {
				t1_node_idx_for_vba(req._vba, parent_lvl, req._snap_degr) };

			if (!check_sha256_4k_hash(chan._encoded_blk,
			                          chan._t1_blks.items[parent_lvl].nodes[child_idx].hash)) {

				chan._mark_req_failed(progress, "check inner node hash");
				break;
			}
		}
		if (chan._lvl > 1) {

			Tree_level_index const parent_lvl { chan._lvl };
			Tree_level_index const child_lvl  { parent_lvl - 1 };
			Tree_node_index  const child_idx  {
				t1_node_idx_for_vba(req._vba, parent_lvl, req._snap_degr) };

			Type_1_node const &child { chan._t1_blks.items[parent_lvl].nodes[child_idx] };

			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("lvl ", parent_lvl, " node ", child_idx, ": "), child);

			if (!chan._first_snapshot &&
			    chan._t1_blks_old_pbas.items[child_lvl] == child.pba) {

				/*
				 * The rest of this branch has already been rekeyed while
				 * rekeying the vba at another snapshot and can therefore be
				 * skipped.
				 */
				chan._lvl = child_lvl;
				_set_args_for_alloc_of_new_pbas_for_rekeying(chan, parent_lvl);
				chan._generate_ft_req(Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_SUCCEEDED, progress, chan._ft_rkg_alloc_type());
				if (VERBOSE_REKEYING)
					log("        [child already rekeyed at pba ", chan._new_pbas.pbas[child_lvl], "]");

			} else {
				chan._lvl = child_lvl;
				chan._t1_blks_old_pbas.items[child_lvl] = child.pba;
				chan._generate_req<Block_io::Read>(Channel::READ_INNER_NODE_SUCCEEDED, progress, child.pba, chan._encoded_blk);
			}

		} else {

			Tree_level_index const parent_lvl { chan._lvl };
			Tree_node_index  const child_idx  {
				t1_node_idx_for_vba(req._vba, parent_lvl, req._snap_degr) };

			Type_1_node const &child { chan._t1_blks.items[parent_lvl].nodes[child_idx] };

			if (VERBOSE_REKEYING)
				log("        ", Branch_lvl_prefix("lvl ", parent_lvl, " node ", child_idx, ": "), child);

			if (!chan._first_snapshot
			    && chan._data_blk_old_pba == child.pba) {

				/*
				 * The leaf node of this branch has already been rekeyed while
				 * rekeying the vba at another snapshot and can therefore be
				 * skipped.
				 */
				_set_args_for_alloc_of_new_pbas_for_rekeying(chan, parent_lvl);
				chan._generate_ft_req(Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED, progress, chan._ft_rkg_alloc_type());
				if (VERBOSE_REKEYING)
					log("        [child already rekeyed at pba ", chan._new_pbas.pbas[0], "]");

			} else if (child.gen == INITIAL_GENERATION) {

				/*
				 * The leaf node of this branch is still unused and can
				 * therefore be skipped because the driver will yield all
				 * zeroes for it regardless of the used key.
				 */
				_set_args_for_alloc_of_new_pbas_for_rekeying(chan, 0);
				chan._generate_ft_req(Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED, progress, chan._ft_rkg_alloc_type());
				if (VERBOSE_REKEYING)
					log("        [child needs no rekeying]");

			} else {
				chan._data_blk_old_pba = child.pba;
				chan._generate_req<Block_io::Read>(Channel::READ_LEAF_NODE_SUCCEEDED, progress, child.pba, chan._data_blk);
			}
		}
		break;
	}
	case Channel::READ_LEAF_NODE_SUCCEEDED:
	{
		Tree_level_index const parent_lvl { FIRST_T1_NODE_BLKS_IDX };
		Tree_node_index  const child_idx  {
			t1_node_idx_for_vba(req._vba, parent_lvl, req._snap_degr) };

		Type_1_node &node {
			chan._t1_blks.items[parent_lvl].nodes[child_idx] };

		if (!check_sha256_4k_hash(chan._data_blk, node.hash)) {

			chan._mark_req_failed(progress, "check leaf node hash");
			break;
		}
		chan._generate_req<Crypto::Decrypt>(
			Channel::DECRYPT_LEAF_NODE_SUCCEEDED, progress, req._prev_key_id, chan._data_blk_old_pba, chan._data_blk);

		if (VERBOSE_REKEYING)
			log("        ", Branch_lvl_prefix("leaf data: "), chan._data_blk);

		break;
	}
	case Channel::DECRYPT_LEAF_NODE_SUCCEEDED:

		_set_args_for_alloc_of_new_pbas_for_rekeying(chan, 0);
		chan._generate_ft_req(Channel::ALLOC_PBAS_AT_LEAF_LVL_SUCCEEDED, progress, chan._ft_rkg_alloc_type());
		if (VERBOSE_REKEYING) {
			Hash hash { };
			calc_sha256_4k_hash(chan._data_blk, hash);
			log("      re-encrypt leaf data: plaintext ", chan._data_blk, " hash ", hash);
		}
		break;

	case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED:

		chan._log_rekeying_pba_alloc();

		if (VERBOSE_REKEYING)
			log("      update branch:");

		chan._state = Channel::WRITE_LEAF_NODE_SUCCEEDED;
		progress = true;
		break;

	case Channel::ALLOC_PBAS_AT_LEAF_LVL_SUCCEEDED:

		chan._log_rekeying_pba_alloc();
		chan._generate_req<Crypto::Encrypt>(
			Channel::ENCRYPT_LEAF_NODE_SUCCEEDED, progress, req._curr_key_id, chan._new_pbas.pbas[0], chan._data_blk);
		break;

	case Channel::ENCRYPT_LEAF_NODE_SUCCEEDED:
	{
		chan._generate_req<Block_io::Write>(Channel::WRITE_LEAF_NODE_SUCCEEDED, progress, chan._new_pbas.pbas[0], chan._data_blk);
		if (VERBOSE_REKEYING) {
			log("      update branch:");
			log("        ", Branch_lvl_prefix("leaf data: "), chan._data_blk);
		}
		break;
	}
	case Channel::WRITE_LEAF_NODE_SUCCEEDED:
	{
		Tree_level_index       const parent_lvl { 1 };
		Tree_level_index       const child_lvl  { 0 };
		Physical_block_address const child_pba  { chan._new_pbas.pbas[child_lvl] };
		Physical_block_address const parent_pba { chan._new_pbas.pbas[parent_lvl] };
		Tree_node_index        const child_idx  {
			t1_node_idx_for_vba(req._vba, parent_lvl, req._snap_degr) };

		Type_1_node &node { chan._t1_blks.items[parent_lvl].nodes[child_idx] };
		node.pba = child_pba;
		calc_sha256_4k_hash(chan._data_blk, node.hash);

		if (VERBOSE_REKEYING)
			log("        ", Branch_lvl_prefix("lvl ", parent_lvl, " node ", child_idx, ": "), node);

		chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
		chan._generate_req<Block_io::Write>(Channel::WRITE_INNER_NODE_SUCCEEDED, progress, parent_pba, chan._encoded_blk);
		break;
	}
	case Channel::WRITE_INNER_NODE_SUCCEEDED:
	{
		Snapshot               const &snap       { (req._snapshots).items[chan._snap_idx] };
		Tree_level_index       const  parent_lvl { chan._lvl + 1 };
		Tree_level_index       const  child_lvl  { chan._lvl };
		Physical_block_address const  child_pba  { chan._new_pbas.pbas[child_lvl] };
		Physical_block_address const  parent_pba { chan._new_pbas.pbas[parent_lvl] };
		Tree_node_index        const  child_idx  {
			t1_node_idx_for_vba(req._vba, parent_lvl, req._snap_degr) };;

		Type_1_node &node { chan._t1_blks.items[parent_lvl].nodes[child_idx] };
		node.pba = child_pba;
		calc_sha256_4k_hash(chan._encoded_blk, node.hash);

		if (VERBOSE_REKEYING)
			log("        ", Branch_lvl_prefix("lvl ", parent_lvl, " node ", child_idx, ": "), node);

		chan._lvl++;
		if (chan._lvl < snap.max_level) {
			chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
			chan._generate_req<Block_io::Write>(Channel::WRITE_INNER_NODE_SUCCEEDED, progress, parent_pba, chan._encoded_blk);
		} else {
			chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
			chan._generate_req<Block_io::Write>(Channel::WRITE_ROOT_NODE_SUCCEEDED, progress, parent_pba, chan._encoded_blk);
		}
		progress = true;
		break;
	}
	case Channel::WRITE_ROOT_NODE_SUCCEEDED:
	{
		Snapshot                     &snap      { (req._snapshots).items[chan._snap_idx] };
		Tree_level_index       const  child_lvl { chan._lvl };
		Physical_block_address const  child_pba { chan._new_pbas.pbas[child_lvl] };

		snap.pba = child_pba;
		calc_sha256_4k_hash(chan._encoded_blk, snap.hash);

		if (VERBOSE_REKEYING)
			log("        ", Branch_lvl_prefix("root: "), snap);

		Snapshot_index next_snap_idx { 0 };
		if (chan._find_next_snap_to_rekey_vba_at(next_snap_idx)) {

			chan._snap_idx = next_snap_idx;
			Snapshot const &snap { (req._snapshots).items[chan._snap_idx] };

			chan._first_snapshot = false;
			chan._lvl = snap.max_level;
			if (chan._t1_blks_old_pbas.items[chan._lvl] == snap.pba) {

				progress = true;

			} else {

				chan._t1_blks_old_pbas.items[chan._lvl] = snap.pba;
				chan._generate_req<Block_io::Read>(Channel::READ_ROOT_NODE_SUCCEEDED, progress, snap.pba, chan._encoded_blk);
				if (VERBOSE_REKEYING) {
					log("    snapshot ", chan._snap_idx, ":");
					log("      load branch:");
					log("        ", Branch_lvl_prefix("root: "), snap);
				}
			}

		} else {

			chan._mark_req_successful(progress);
		}
		break;
	}
	case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_SUCCEEDED:

		chan._log_rekeying_pba_alloc();
		chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
		chan._state = Channel::WRITE_INNER_NODE_SUCCEEDED;

		if (VERBOSE_REKEYING)
			log("      update branch:");

		progress = true;

	default:

		break;
	}
}


void Virtual_block_device::
_add_new_root_lvl_to_snap_using_pba_contingent(Channel &chan)
{
	Request                &req     { *chan._req_ptr };
	Snapshot_index const    old_idx { chan._snap_idx };
	Snapshot_index         &idx     { chan._snap_idx };
	Snapshot               *snap    { (req._snapshots).items };
	Physical_block_address  new_pba;

	if (snap[idx].max_level == TREE_MAX_LEVEL) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Tree_level_index const new_lvl { snap[old_idx].max_level + 1 };
	chan._t1_blks.items[new_lvl] = { };
	chan._t1_blks.items[new_lvl].nodes[0] =
		{ snap[idx].pba, snap[idx].gen, snap[idx].hash };

	if (snap[idx].gen < req._curr_gen) {

		idx =
			(req._snapshots).alloc_idx(
				req._curr_gen, req._last_secured_generation);

		if (VERBOSE_VBD_EXTENSION)
			log("  new snap ", idx);
	}

	_alloc_pba_from_resizing_contingent(
		req._pba, req._nr_of_pbas, new_pba);

	snap[idx] = {
		Hash { }, new_pba, req._curr_gen, snap[old_idx].nr_of_leaves,
		new_lvl, true, 0, false };

	if (VERBOSE_VBD_EXTENSION) {
		log("  update snap ", idx, " ", snap[idx]);
		log("  update lvl ", new_lvl,
		    " child 0 ", chan._t1_blks.items[new_lvl].nodes[0]);
	}
}


void Virtual_block_device::
_alloc_pba_from_resizing_contingent(Physical_block_address &first_pba,
                                    Number_of_blocks       &nr_of_pbas,
                                    Physical_block_address &allocated_pba)
{
	if (nr_of_pbas == 0) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	allocated_pba = first_pba;
	first_pba++;
	nr_of_pbas--;
}


void Virtual_block_device::
_add_new_branch_to_snap_using_pba_contingent(Channel          &chan,
                                             Tree_level_index  mount_at_lvl,
                                             Tree_node_index   mount_at_child_idx)
{
	Request &req { *chan._req_ptr };
	req._nr_of_leaves = 0;
	chan._lvl = mount_at_lvl;

	/* reset all levels below mount point */
	if (mount_at_lvl > 1) {
		for (Tree_level_index lvl { 1 }; lvl < mount_at_lvl; lvl++)
			chan._t1_blks.items[lvl] = { };
	}
	if (!req._nr_of_pbas)
		return;

	/* set child PBAs of new branch */
	for (Tree_level_index lvl { mount_at_lvl }; lvl > 0; lvl--) {

		chan._lvl = lvl;
		Tree_node_index child_idx {
			lvl == mount_at_lvl ? mount_at_child_idx : 0 };

		auto add_child_at_curr_lvl_and_child_idx = [&] () {

			if (!req._nr_of_pbas)
				return false;

			Physical_block_address child_pba;
			_alloc_pba_from_resizing_contingent(
				req._pba, req._nr_of_pbas, child_pba);

			Type_1_node &child { chan._t1_blks.items[lvl].nodes[child_idx] };
			child = { child_pba, INITIAL_GENERATION, Hash { } };

			if (VERBOSE_VBD_EXTENSION)
				log("  update lvl ", lvl, " child ", child_idx, " ", child);

			return true;
		};
		if (lvl > 1) {

			if (!add_child_at_curr_lvl_and_child_idx())
				return;

		} else {

			for (; child_idx < req._snap_degr; child_idx++) {

				if (!add_child_at_curr_lvl_and_child_idx())
					return;

				(req._nr_of_leaves)++;
			}
		}
	}
}


void
Virtual_block_device::_set_new_pbas_identical_to_current_pbas(Channel &chan)
{
	Request &req { *chan._req_ptr };
	Snapshot &snap { (chan._req_ptr->_snapshots).items[chan._snap_idx] };

	for (Tree_level_index lvl { 0 }; lvl <= TREE_MAX_LEVEL; lvl++) {

		if (lvl > snap.max_level) {

			chan._new_pbas.pbas[lvl] = 0;

		} else if (lvl == snap.max_level) {

			chan._new_pbas.pbas[lvl] = snap.pba;

		} else {

			Tree_node_index const child_idx {
				t1_node_idx_for_vba(chan._vba, lvl + 1, req._snap_degr) };

			Type_1_node const &child {
				chan._t1_blks.items[lvl + 1].nodes[child_idx] };

			chan._new_pbas.pbas[lvl] = child.pba;
		}
	}
}


void Virtual_block_device::
_set_args_for_alloc_of_new_pbas_for_resizing(Channel          &chan,
                                             Tree_level_index  min_lvl,
                                             bool             &progress)
{
	Request const &req { *chan._req_ptr };
	Snapshot const &snap { (req._snapshots).items[chan._snap_idx] };

	if (min_lvl > snap.max_level) {

		chan._mark_req_failed(progress, "check parent lvl for alloc");
		return;
	}
	chan._nr_of_blks = 0;
	chan._free_gen = req._curr_gen;
	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {

		if (lvl > snap.max_level) {

			chan._new_pbas.pbas[lvl] = 0;
			chan._t1_node_walk.nodes[lvl] = { };

		} else if (lvl == snap.max_level) {

			chan._nr_of_blks++;
			chan._new_pbas.pbas[lvl] = 0;
			chan._t1_node_walk.nodes[lvl] = {
				snap.pba, snap.gen, snap.hash };

		} else {

			Tree_node_index const child_idx {
				t1_node_idx_for_vba(
					chan._vba, lvl + 1, req._snap_degr) };

			Type_1_node const &child {
				chan._t1_blks.items[lvl + 1].nodes[child_idx] };

			if (lvl >= min_lvl) {

				chan._nr_of_blks++;
				chan._new_pbas.pbas[lvl] = 0;
				chan._t1_node_walk.nodes[lvl] = child;

			} else {

				/*
				 * FIXME
				 *
				 * This is done only because the Free Tree module would
				 * otherwise get stuck. It is normal that the lowest
				 * levels have PBA 0 when creating the new branch
				 * stopped at an inner node because of a depleted PBA
				 * contingent. As soon as the strange behavior in the
				 * Free Tree module has been fixed, the whole 'if'
				 * statement can be removed.
				 */
				if (child.pba == 0) {

					chan._new_pbas.pbas[lvl] = INVALID_PBA;
					chan._t1_node_walk.nodes[lvl] = {
						INVALID_PBA, child.gen, child.hash };

				} else {

					chan._new_pbas.pbas[lvl] = child.pba;
					chan._t1_node_walk.nodes[lvl] = child;
				}
			}
		}
	}
	chan._generate_ft_req(Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED, progress, Free_tree_request::ALLOC_FOR_NON_RKG);
}


void Virtual_block_device_channel::_request_submitted(Module_request &req)
{
	_req_ptr = static_cast<Request *>(&req);
	_state = SUBMITTED;
}


void Virtual_block_device::_execute_vbd_extension_step(Channel  &chan,
                                                       bool     &progress)
{
	Request &req { *chan._req_ptr };

	switch (chan._state) {
	case Channel::State::SUBMITTED:
	{
		req._nr_of_leaves = 0;
		chan._snap_idx = (req._snapshots).newest_snapshot_idx();

		chan._vba = chan.snap().nr_of_leaves;
		chan._lvl = chan.snap().max_level;
		chan._t1_blks_old_pbas.items[chan._lvl] = chan.snap().pba;

		if (chan._vba <= tree_max_max_vba(req._snap_degr, chan.snap().max_level)) {

			if (VERBOSE_VBD_EXTENSION)
				log("  read lvl ", chan._lvl,
				    " parent snap ", chan._snap_idx,
				    " ", chan.snap());

			chan._generate_req<Block_io::Read>(Channel::READ_ROOT_NODE_SUCCEEDED, progress, chan.snap().pba, chan._encoded_blk);

		} else {

			_add_new_root_lvl_to_snap_using_pba_contingent(chan);
			_add_new_branch_to_snap_using_pba_contingent(chan, (req._snapshots).items[chan._snap_idx].max_level, 1);
			_set_new_pbas_identical_to_current_pbas(chan);
			chan._generate_write_node_req(progress);

			if (VERBOSE_VBD_EXTENSION)
				log("  write 1 lvl ", chan._lvl, " pba ",
				    (Physical_block_address)chan._new_pbas.pbas[chan._lvl]);
		}
		break;
	}
	case Channel::READ_ROOT_NODE_SUCCEEDED:
	case Channel::READ_INNER_NODE_SUCCEEDED:
	{
		chan._t1_blks.items[chan._lvl].decode_from_blk(chan._encoded_blk);
		if (chan._lvl == chan.snap().max_level) {

			if (!check_sha256_4k_hash(chan._encoded_blk, chan.snap().hash)) {

				chan._mark_req_failed(progress, "check root node hash");
				break;
			}

		} else {

			Tree_level_index const parent_lvl { chan._lvl + 1 };
			Tree_node_index  const child_idx  {
				t1_node_idx_for_vba(chan._vba, parent_lvl, req._snap_degr) };

			if (!check_sha256_4k_hash(chan._encoded_blk,
			                          chan._t1_blks.items[parent_lvl].nodes[child_idx].hash)) {

				chan._mark_req_failed(progress, "check inner node hash");
				break;
			}
		}
		if (chan._lvl > 1) {

			Tree_level_index const parent_lvl { chan._lvl };
			Tree_level_index const child_lvl  { parent_lvl - 1 };
			Tree_node_index  const child_idx  {
				t1_node_idx_for_vba(chan._vba, parent_lvl, req._snap_degr) };

			Type_1_node const &child { chan._t1_blks.items[parent_lvl].nodes[child_idx] };

			if (child.valid()) {

				chan._lvl = child_lvl;
				chan._t1_blks_old_pbas.items[child_lvl] = child.pba;
				chan._generate_req<Block_io::Read>(Channel::READ_INNER_NODE_SUCCEEDED, progress, child.pba, chan._encoded_blk);
				if (VERBOSE_VBD_EXTENSION)
					log("  read lvl ", child_lvl, " parent lvl ", parent_lvl,
					    " child ", child_idx, " ", child);

			} else {
				_add_new_branch_to_snap_using_pba_contingent(chan, parent_lvl, child_idx);
				_set_args_for_alloc_of_new_pbas_for_resizing(chan, parent_lvl, progress);
				break;
			}

		} else {

			Tree_level_index const parent_lvl { chan._lvl };
			Tree_node_index  const child_idx  {
				t1_node_idx_for_vba(chan._vba, parent_lvl, req._snap_degr) };

			_add_new_branch_to_snap_using_pba_contingent(chan, parent_lvl, child_idx);
			_set_args_for_alloc_of_new_pbas_for_resizing(chan, parent_lvl, progress);
			break;
		}
		break;
	}
	case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED:
	{
		Physical_block_address const new_pba {
			chan._new_pbas.pbas[chan._lvl] };

		if (VERBOSE_VBD_EXTENSION) {
			log("  allocated ", chan._nr_of_blks, " pbas");
			for (Tree_level_index lvl { 0 }; lvl < chan.snap().max_level; lvl++) {
				log("    lvl ", lvl, " ",
				    chan._t1_node_walk.nodes[lvl], " -> pba ",
				    (Physical_block_address)chan._new_pbas.pbas[lvl]);
			}
			log("  write 1 lvl ", chan._lvl, " pba ", new_pba);
		}
		if (chan._lvl < chan.snap().max_level) {
			chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
			chan._generate_req<Block_io::Write>(Channel::WRITE_INNER_NODE_SUCCEEDED, progress, new_pba, chan._encoded_blk);
		} else {
			chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
			chan._generate_req<Block_io::Write>(Channel::WRITE_ROOT_NODE_SUCCEEDED, progress, new_pba, chan._encoded_blk);
		}
		progress = true;
		break;
	}
	case Channel::WRITE_INNER_NODE_SUCCEEDED:
	{
		Tree_level_index       const  parent_lvl { chan._lvl + 1 };
		Tree_level_index       const  child_lvl  { chan._lvl };
		Tree_node_index        const  child_idx  { t1_node_idx_for_vba(chan._vba, parent_lvl, req._snap_degr) };
		Physical_block_address const  child_pba  { chan._new_pbas.pbas[child_lvl] };
		Physical_block_address const  parent_pba { chan._new_pbas.pbas[parent_lvl] };
		Type_1_node                  &child      { chan._t1_blks.items[parent_lvl].nodes[child_idx] };

		calc_sha256_4k_hash(chan._encoded_blk, child.hash);
		child.pba = child_pba;

		if (VERBOSE_VBD_EXTENSION) {
			log("  update lvl ", parent_lvl, " child ", child_idx, " ", child);
			log("  write 2 lvl ", parent_lvl, " pba ", parent_pba);
		}
		chan._lvl++;
		if (chan._lvl < chan.snap().max_level) {
			chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
			chan._generate_req<Block_io::Write>(Channel::WRITE_INNER_NODE_SUCCEEDED, progress, parent_pba, chan._encoded_blk);
		} else {
			chan._t1_blks.items[chan._lvl].encode_to_blk(chan._encoded_blk);
			chan._generate_req<Block_io::Write>(Channel::WRITE_ROOT_NODE_SUCCEEDED, progress, parent_pba, chan._encoded_blk);
		}
		progress = true;
		break;
	}
	case Channel::WRITE_ROOT_NODE_SUCCEEDED:
	{
		Tree_level_index       const  child_lvl { chan._lvl };
		Physical_block_address const  child_pba { chan._new_pbas.pbas[child_lvl] };
		Snapshot               const &old_snap  { (req._snapshots).items[chan._snap_idx] };

		if (old_snap.gen < req._curr_gen) {

			chan._snap_idx =
				(req._snapshots).alloc_idx(
					req._curr_gen, req._last_secured_generation);

			if (VERBOSE_VBD_EXTENSION)
				log("  new snap ", chan._snap_idx);
		}

		Snapshot &new_snap { (req._snapshots).items[chan._snap_idx] };
		new_snap = {
			Hash { }, child_pba, req._curr_gen,
			old_snap.nr_of_leaves + req._nr_of_leaves, old_snap.max_level,
			true, 0, false };

		calc_sha256_4k_hash(chan._encoded_blk, new_snap.hash);

		if (VERBOSE_VBD_EXTENSION)
			log("  update snap ", chan._snap_idx, " ", new_snap);

		chan._mark_req_successful(progress);
		break;
	}
	default:

		break;
	}
}


void Virtual_block_device::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		if (!chan._req_ptr)
			return;

		switch (chan._req_ptr->_type) {
		case Request::READ_VBA: chan._read_vba(progress); break;
		case Request::WRITE_VBA: chan._write_vba(progress); break;
		case Request::REKEY_VBA: _execute_rekey_vba(chan, progress); break;
		case Request::VBD_EXTENSION_STEP: _execute_vbd_extension_step(chan, progress); break;
		}
	});
}

void Virtual_block_device_channel::_generate_ft_req(State complete_state, bool progress, Free_tree_request::Type type)
{
	Request &req { *_req_ptr };
	_generate_req<Free_tree_request>(
		complete_state, progress, type, req._ft, req._mt, req._snapshots,
		req._last_secured_generation, req._curr_gen, _free_gen, _nr_of_blks, _new_pbas,
		_t1_node_walk, req._snapshots.items[_snap_idx].max_level, _vba, req._vbd_degree,
		req._vbd_highest_vba, req._rekeying, req._prev_key_id, req._curr_key_id, _vba);
}
