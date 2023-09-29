/*
 * \brief  Module for doing VBD COW allocations on the free tree
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
#include <tresor/free_tree.h>
#include <tresor/meta_tree.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

void Free_tree_channel::_generate_mt_req(State_uint state, bool &progress, Physical_block_address &pba)
{
	_state = REQ_GENERATED;
	generate_req<Meta_tree_request>(
		state, progress, Meta_tree_request::ALLOC_PBA, _req_ptr->_mt, _req_ptr->_curr_gen, pba, _generated_req_success);
}


static Virtual_block_address
vbd_node_min_vba(Tree_degree_log_2 vbd_degr_log_2, Tree_level_index vbd_lvl, Virtual_block_address vbd_leaf_vba)
{
	return vbd_leaf_vba & (~(Physical_block_address)0 << ((Physical_block_address)vbd_degr_log_2 * vbd_lvl));
}


static Number_of_blocks vbd_node_num_vbas(Tree_degree_log_2 vbd_degr_log_2, Tree_level_index vbd_lvl)
{
	return (Number_of_blocks)1 << ((Number_of_blocks)vbd_degr_log_2 * vbd_lvl);
}


static Virtual_block_address
vbd_node_max_vba(Tree_degree_log_2 vbd_degr_log_2, Tree_level_index vbd_lvl, Virtual_block_address vbd_leaf_vba)
{
	return vbd_node_num_vbas(vbd_degr_log_2, vbd_lvl) - 1 + vbd_node_min_vba(vbd_degr_log_2, vbd_lvl, vbd_leaf_vba);
}


char const *Free_tree_request::type_to_string(Type type)
{
	switch (type) {
	case ALLOC_FOR_NON_RKG: return "alloc_for_non_rkg";
	case ALLOC_FOR_RKG_CURR_GEN_BLKS: return "alloc_for_rkg_curr_gen_blks";
	case ALLOC_FOR_RKG_OLD_GEN_BLKS: return "alloc_for_rkg_old_gen_blks";
	}
	return "?";
}


Free_tree_request::Free_tree_request(Module_id src_module_id, Module_request_id src_request_id, Type type,
                                     Free_tree_root &ft, Meta_tree_root &mt, Snapshots const &snapshots, Generation last_secured_gen,
                                     Generation curr_gen, Generation free_gen, Number_of_blocks num_required_pbas,
                                     Tree_walk_pbas &new_blocks, Type_1_node_walk const &old_blocks,
                                     Tree_level_index max_lvl, Virtual_block_address vba, Tree_degree vbd_degree,
                                     Virtual_block_address vbd_max_vba, bool rekeying, Key_id prev_key_id,
                                     Key_id curr_key_id, Virtual_block_address rekeying_vba, bool &success)

:
	Module_request { src_module_id, src_request_id, FREE_TREE }, _type { type }, _ft { ft }, _mt { mt },
	_curr_gen { curr_gen }, _free_gen { free_gen }, _num_required_pbas { num_required_pbas }, _new_blocks { new_blocks },
	_old_blocks { old_blocks }, _max_lvl { max_lvl }, _vba { vba }, _vbd_degree { vbd_degree }, _vbd_max_vba { vbd_max_vba },
	_rekeying { rekeying }, _prev_key_id { prev_key_id }, _curr_key_id { curr_key_id }, _rekeying_vba { rekeying_vba },
	_success { success }, _snapshots { snapshots },
	_last_secured_gen { last_secured_gen }
{ }


void Free_tree::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


bool Free_tree_channel::_can_alloc_pba_of(Type_2_node &node)
{
	Request &req { *_req_ptr };
	if (node.pba == 0 || node.pba == INVALID_PBA || node.free_gen > req._last_secured_gen)
		return false;

	if (!node.reserved)
		return true;

	if (req._rekeying && node.last_key_id == req._prev_key_id && node.last_vba < req._rekeying_vba)
		return true;

	for (Snapshot const &snap : req._snapshots.items)
		if (snap.valid && node.free_gen > snap.gen && node.alloc_gen < snap.gen + 1)
			return false;

	return true;
}


void Free_tree_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		return;
	}
	_state = (State)state_uint;
}


void Free_tree_channel::_traverse_tree(bool &progress)
{
	Request &req { *_req_ptr };
	while (1) {
		if (_lvl) {
			Type_1_node &t1_node = _t1_blks[_lvl].nodes[_node_idx[_lvl]];
			switch (_node_state[_lvl]) {
			case SUBTREE_NOT_TRAVERSED:

				if (!t1_node.pba) {
					_node_state[_lvl] = SUBTREE_TRAVERSED;
					break;
				}
				_generate_cache_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, t1_node.pba, _blk);
				return;

			case SUBTREE_ROOT_BLK_READ:

				_node_state[_lvl] = _alloc_pbas ? SUBTREE_MODIFIED : SUBTREE_TRAVERSED;
				_lvl--;
				if (_lvl)
					_t1_blks[_lvl].decode_from_blk(_blk);
				else
					_t2_blk.decode_from_blk(_blk);
				_node_idx[_lvl] = req._ft.degree - 1;
				_node_state[_lvl] = SUBTREE_NOT_TRAVERSED;
				break;

			case SUBTREE_MODIFIED:
			{
				ASSERT(_alloc_pbas);
				_node_state[_lvl] = SUBTREE_ROOT_BLK_READY_FOR_WRITE;
				if (!t1_node.is_volatile(req._curr_gen)) {
					_generate_mt_req(ALLOC_PBA_SUCCEEDED, progress, t1_node.pba);
					return;
				}
				break;
			}
			case SUBTREE_ROOT_BLK_READY_FOR_WRITE:
			{
				ASSERT(_alloc_pbas);
				if (_lvl > 1)
					_t1_blks[_lvl - 1].encode_to_blk(_blk);
				else
					_t2_blk.encode_to_blk(_blk);
				t1_node.gen = req._curr_gen;
				calc_hash(_blk, t1_node.hash);
				_generate_cache_req<Block_io::Write>(WRITE_BLK_SUCCEEDED, progress, t1_node.pba, _blk);
				return;
			}
			case SUBTREE_TRAVERSED:

				if (_lvl == req._ft.max_lvl) {
					if (!_alloc_pbas) {
						if (_num_pbas < req._num_required_pbas)
							_mark_req_failed(progress, "not enough free pbas");
						else {
							_alloc_pbas = true;
							_start_tree_traversal(progress);
						}
					} else {
						req._ft.t1_node(t1_node);
						_mark_req_successful(progress);
					}
					return;
				}
				_advance_to_next_node();
				break;
			}
		} else {
			Type_2_node &t2_node { _t2_blk.nodes[_node_idx[_lvl]] };
			if (_num_pbas < req._num_required_pbas && _can_alloc_pba_of(t2_node)) {
				if (_alloc_pbas)
					_alloc_pba_of(t2_node);
				_num_pbas++;
			}
			_advance_to_next_node();
		}
	}
}


void Free_tree_channel::_advance_to_next_node()
{
	if (_node_idx[_lvl] && _num_pbas < _req_ptr->_num_required_pbas) {
		_node_idx[_lvl]--;
		_node_state[_lvl] = SUBTREE_NOT_TRAVERSED;
	} else
		_lvl++;
}


void Free_tree_channel::_alloc_pba_of(Type_2_node &t2_node)
{
	Request &req { *_req_ptr };
	Tree_level_index vbd_lvl { 0 };
	for (; vbd_lvl <= req._max_lvl && req._new_blocks.pbas[vbd_lvl]; vbd_lvl++);

	Virtual_block_address node_min_vba { vbd_node_min_vba(_vbd_degree_log_2, vbd_lvl, req._vba) };
	req._new_blocks.pbas[vbd_lvl] = t2_node.pba;
	t2_node.alloc_gen = req._old_blocks.nodes[vbd_lvl].gen;
	t2_node.free_gen = req._free_gen;
	Virtual_block_address rkg_vba { req._rekeying_vba };
	switch (req._type) {
	case Request::ALLOC_FOR_NON_RKG:

		t2_node.reserved = true;
		t2_node.pba = req._old_blocks.nodes[vbd_lvl].pba;
		t2_node.last_vba = node_min_vba;
		if (req._rekeying) {
			if (req._vba < rkg_vba)
				t2_node.last_key_id = req._curr_key_id;
			else
				t2_node.last_key_id = req._prev_key_id;
		} else
			t2_node.last_key_id = req._curr_key_id;
		break;

	case Request::ALLOC_FOR_RKG_CURR_GEN_BLKS:

		t2_node.reserved = false;
		t2_node.pba = req._old_blocks.nodes[vbd_lvl].pba;
		t2_node.last_vba = node_min_vba;
		t2_node.last_key_id = req._prev_key_id;
		break;

	case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS:
	{
		t2_node.reserved = true;
		Virtual_block_address node_max_vba { vbd_node_max_vba(_vbd_degree_log_2, vbd_lvl, req._vba) };
		if (rkg_vba < node_max_vba && rkg_vba < req._vbd_max_vba) {
			t2_node.last_key_id = req._prev_key_id;
			t2_node.last_vba = rkg_vba + 1;
		} else if (rkg_vba == node_max_vba || rkg_vba == req._vbd_max_vba) {
			t2_node.last_key_id = req._curr_key_id;
			t2_node.last_vba = node_min_vba;
		} else
			ASSERT_NEVER_REACHED;
		break;
	}
	default: ASSERT_NEVER_REACHED;
	}
}


void Free_tree_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"", str, "\"");
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Free_tree_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = COMPLETE;
	progress = true;
}


void Free_tree_channel::_start_tree_traversal(bool &progress)
{
	Request &req { *_req_ptr };
	_num_pbas = 0;
	_lvl = req._ft.max_lvl;
	_node_idx[_lvl] = 0;
	_t1_blks[_lvl].nodes[_node_idx[_lvl]] = req._ft.t1_node();
	_node_state[_lvl] = SUBTREE_NOT_TRAVERSED;
	_generate_cache_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, req._ft.pba, _blk);
}


void Free_tree_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_vbd_degree_log_2 = log2<Tree_degree_log_2>(req._vbd_degree);
		_alloc_pbas = false;
		_start_tree_traversal(progress);
		break;

	case READ_BLK_SUCCEEDED:
	{
		if (!check_hash(_blk, _t1_blks[_lvl].nodes[_node_idx[_lvl]].hash)) {
			_mark_req_failed(progress, "hash mismatch");
			break;
		}
		_node_state[_lvl] = SUBTREE_ROOT_BLK_READ;
		_traverse_tree(progress);
		break;
	}
	case WRITE_BLK_SUCCEEDED:

		_node_state[_lvl] = SUBTREE_TRAVERSED;
		_traverse_tree(progress);
		break;

	case ALLOC_PBA_SUCCEEDED: _traverse_tree(progress); break;
	default: break;
	}
}


void Free_tree_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Free_tree::Free_tree()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}
