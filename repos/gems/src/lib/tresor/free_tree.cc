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

void Free_tree_channel::_generate_mt_req(State_uint state, bool &progress, Physical_block_address pba)
{
	ASSERT(_state == UPDATE_REQ_INVALID);
	_state = UPDATE_REQ_GENERATED;
	generate_req<Meta_tree_request>(
		state, progress, Meta_tree_request::ALLOC_PBA, _req_ptr->_mt, _req_ptr->_curr_gen, pba, _pba, _generated_req_success);
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


Free_tree_request::Free_tree_request(Module_id src_module_id,
                  Module_request_id src_request_id,
                  Type type,
                  Free_tree_root &ft,
                  Meta_tree_root &mt,
                  Snapshots const &snapshots,
                  Generation last_secured_gen,
                  Generation curr_gen,
                  Generation free_gen,
                  Number_of_blocks num_required_pbas,
                  Tree_walk_pbas &new_blocks,
                  Type_1_node_walk const &old_blocks,
                  Tree_level_index max_lvl,
                  Virtual_block_address vba,
                  Tree_degree vbd_degree,
                  Virtual_block_address vbd_max_vba,
                  bool rekeying,
                  Key_id prev_key_id,
                  Key_id curr_key_id,
                  Virtual_block_address rekeying_vba,
                  bool &success)
:
	Module_request { src_module_id, src_request_id, FREE_TREE },
	_type { type },
	_ft { ft },
	_mt { mt },
	_curr_gen { curr_gen },
	_free_gen { free_gen },
	_num_required_pbas { num_required_pbas },
	_new_blocks { new_blocks },
	_old_blocks { old_blocks },
	_max_lvl { max_lvl },
	_vba { vba },
	_vbd_degree { vbd_degree },
	_vbd_max_vba { vbd_max_vba },
	_rekeying { rekeying },
	_prev_key_id { prev_key_id },
	_curr_key_id { curr_key_id },
	_rekeying_vba { rekeying_vba },
	_success { success },
	_snapshots { snapshots },
	_last_secured_gen { last_secured_gen }
{ }


void Free_tree::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


void Free_tree_channel::_init_info_stack_from_blk_data(Tree_level_index lvl)
{
	if (lvl) {
		_t1_blks[lvl].decode_from_blk(_blk);
		_t1_info_stacks[lvl].reset();
		for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
			if (_t1_blks[lvl].nodes[idx].pba != 0) {
				_t1_info_stacks[lvl].push({
					SUBTREE_NOT_TRAVERSED, _t1_blks[lvl].nodes[idx], idx,
					_t1_blks[lvl].nodes[idx].is_volatile(_req_ptr->_curr_gen) });
			}
		}
	} else {
		_t2_blk.decode_from_blk(_blk);
		_t2_info_stack.reset();
		for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
			if (_t2_node_allocable(_t2_blk.nodes[idx]))
				_t2_info_stack.push({ SUBTREE_NOT_TRAVERSED, _t2_blk.nodes[idx], idx });
		}
	}
}


bool Free_tree_channel::_t2_node_allocable(Type_2_node &node)
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
	switch (_state) {
	case SCAN_READ_BLK_SUCCEEDED: ASSERT_NEVER_REACHED;
	case REQ_GENERATED:
		if (!_generated_req_success) {
			error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
			_req_ptr->_success = false;
			_state = COMPLETE;
			return;
		}
		_state = (State)state_uint;
		break;
	default:
		ASSERT(_state == UPDATE_REQ_GENERATED);
		if (!_generated_req_success) {
			error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
			_req_ptr->_success = false;
			_state = COMPLETE;
			return;
		}
		_state = (State)state_uint;
		switch (_state) {
		case UPDATE_READ_BLK_SUCCEEDED:
		{
			Type_1_info &t1_info { _t1_info_stacks[_lvl].top() };
			if (check_hash(_blk, t1_info.node.hash)) {
				t1_info.state = SUBTREE_ROOT_BLK_READ;
			} else {
				error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"node hash mismatch\"");
				_req_ptr->_success = false;
				_state = COMPLETE;
			}
			break;
		}
		case UPDATE_WRITE_BLK_SUCCEEDED:

			_t1_info_stacks[_lvl].top().state = SUBTREE_TRAVERSED;
			break;

		case UPDATE_ALLOC_PBA_SUCCEEDED: break;
		default: ASSERT_NEVER_REACHED;
		}
		break;
	}
}

Tree_level_index Free_tree_channel::_lowest_non_empty_lvl() const
{
	if (!_t2_info_stack.empty())
		return 0;

	for (Tree_level_index lvl = 1; lvl <= _req_ptr->_ft.max_lvl; lvl++)
		if (!_t1_info_stacks[lvl].empty())
			return lvl;

	ASSERT_NEVER_REACHED;
}


void Free_tree_channel::_traverse_tree_to_find_pbas(bool &progress)
{
	Request &req { *_req_ptr };
	while (1) {
		Tree_level_index lvl { _lowest_non_empty_lvl() };
		if (lvl) {
			Type_1_info &t1_info = _t1_info_stacks[lvl].top();
			switch (t1_info.state) {
			case SUBTREE_NOT_TRAVERSED:

				_generate_cache_req<Block_io::Read>(SCAN_READ_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _blk);
				return;

			case SUBTREE_ROOT_BLK_READ:

				_init_info_stack_from_blk_data(lvl - 1);
				t1_info.state = SUBTREE_TRAVERSED;
				break;

			case SUBTREE_TRAVERSED:

				if (lvl == req._ft.max_lvl) {
					_mark_req_failed(progress, "not enough free blocks");
					return;
				} else
					_t1_info_stacks[lvl].pop();
				break;

			default: ASSERT_NEVER_REACHED;
			}
		} else {
			while (!_t2_info_stack.empty()) {
				_t2_info_stack.pop();
				if (++_num_found_pbas < req._num_required_pbas)
					continue;

				_t2_info_stack.reset();
				_t2_blk = { };
				for (Node_info_stack<Type_1_info> &stack : _t1_info_stacks)
					stack.reset();

				for (Type_1_node_block &blk : _t1_blks)
					blk = { };

				Type_1_node root_node { req._ft.pba, req._ft.gen, req._ft.hash };
				_t1_info_stacks[req._ft.max_lvl].push({ SUBTREE_NOT_TRAVERSED, root_node, 0, root_node.is_volatile(req._curr_gen) });
				_state = UPDATE_REQ_INVALID;
				progress = true;
				return;
			}
			_t1_info_stacks[1].top().state = SUBTREE_TRAVERSED;
		}
	}
}


void Free_tree_channel::_try_alloc_pbas_from_lvl_0_stack()
{
	Request &req { *_req_ptr };
	Virtual_block_address rkg_vba { req._rekeying_vba };
	for (Tree_level_index lvl = 0; lvl <= req._max_lvl && !_t2_info_stack.empty(); lvl++) {
		if (req._new_blocks.pbas[lvl] != 0)
			continue;

		Type_2_node &t2_node { _t2_blk.nodes[_t2_info_stack.top().index] };
		Virtual_block_address node_min_vba { vbd_node_min_vba(_vbd_degree_log_2, lvl, req._vba) };
		req._new_blocks.pbas[lvl] = t2_node.pba;
		t2_node.alloc_gen = req._old_blocks.nodes[lvl].gen;
		t2_node.free_gen = req._free_gen;
		switch (req._type) {
		case Request::ALLOC_FOR_NON_RKG:

			t2_node.reserved = true;
			t2_node.pba = req._old_blocks.nodes[lvl].pba;
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
			t2_node.pba = req._old_blocks.nodes[lvl].pba;
			t2_node.last_vba = node_min_vba;
			t2_node.last_key_id = req._prev_key_id;
			break;

		case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS:
		{
			t2_node.reserved = true;
			Virtual_block_address node_max_vba { vbd_node_max_vba(_vbd_degree_log_2, lvl, req._vba) };
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
		_num_allocated_pbas++;
		_t2_info_stack.pop();
	}
}


void Free_tree_channel::_update_t1_node(Type_1_node &t1_node, Type_1_info &t1_info)
{
	t1_node.pba = t1_info.node.pba;
	t1_node.gen = _req_ptr->_curr_gen;
	calc_hash(_blk, t1_node.hash);
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


void Free_tree_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_num_allocated_pbas = 0;
		_num_found_pbas = 0;
		for (Node_info_stack<Type_1_info> &stack : _t1_info_stacks)
			stack.reset();

		for (Type_1_node_block &blk : _t1_blks)
			blk = { };

		_t2_info_stack.reset();
		_t2_blk = { };
		Type_1_node root_node { req._ft.pba, req._ft.gen, req._ft.hash };
		_t1_info_stacks[req._ft.max_lvl].push({ SUBTREE_NOT_TRAVERSED, root_node, 0, root_node.is_volatile(req._curr_gen) });
		_state = SCAN_READ_BLK_SUCCEEDED;
		_vbd_degree_log_2 = log2<Tree_degree_log_2>(req._vbd_degree);
		_generate_cache_req<Block_io::Read>(SCAN_READ_BLK_SUCCEEDED, progress, req._ft.max_lvl, req._ft.pba, _blk);
		break;
	}
	case SCAN_READ_BLK_SUCCEEDED:
	{
		Type_1_info &t1_info { _t1_info_stacks[_lvl].top() };
		if (!check_hash(_blk, t1_info.node.hash)) {
			_mark_req_failed(progress, "hash mismatch");
			break;
		}
		t1_info.state = SUBTREE_ROOT_BLK_READ;
		_traverse_tree_to_find_pbas(progress);
		break;
	}
	case UPDATE_READ_BLK_SUCCEEDED:
	case UPDATE_WRITE_BLK_SUCCEEDED:
	case UPDATE_ALLOC_PBA_SUCCEEDED:
	case UPDATE_REQ_INVALID:
	{
		_traverse_tree_to_alloc_pbas(progress);
		break;
	}
	default: break;
	}
}


void Free_tree_channel::_traverse_tree_to_alloc_pbas(bool &progress)
{
	Request &req { *_req_ptr };
	while (1) {

		bool exchange_finished { false };

		_try_alloc_pbas_from_lvl_0_stack();

		if (_num_allocated_pbas == req._num_required_pbas)
			exchange_finished = true;

		/* handle level 1..N */
		Tree_level_index lvl { 1 };
		for (; lvl <= req._ft.max_lvl; lvl++) {
			if (_t1_info_stacks[lvl].empty())
				continue;
			break;
		}

		Type_1_info &t1_info { _t1_info_stacks[lvl].top() };
		switch (t1_info.state) {
		case SUBTREE_NOT_TRAVERSED:

			_generate_cache_req<Block_io::Read>(UPDATE_READ_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _blk);
			return;

		case SUBTREE_ROOT_BLK_READ:

			_state = UPDATE_REQ_INVALID;
			_init_info_stack_from_blk_data(lvl - 1);
			t1_info.state = _info_stack_empty(lvl - 1) ? SUBTREE_TRAVERSED : X_WRITE;
			break;

		case X_WRITE:
		{
			if (!t1_info.volatil) {
				if (_state == UPDATE_REQ_INVALID) {
					_generate_mt_req(UPDATE_ALLOC_PBA_SUCCEEDED, progress, t1_info.node.pba);
					return;
				} else if (_state == UPDATE_ALLOC_PBA_SUCCEEDED) {
					_state = UPDATE_REQ_INVALID;
					t1_info.volatil = true;
					t1_info.node.pba = _pba;
				} else
					ASSERT_NEVER_REACHED;
			}
			Type_1_node &t1_node { _t1_blks[lvl].nodes[t1_info.index] };
			if (lvl > 1) {
				_t1_blks[lvl - 1].encode_to_blk(_blk);
				if (lvl < req._ft.max_lvl)
					_update_t1_node(t1_node, t1_info);
				else {
					calc_hash(_blk, req._ft.hash);
					req._ft.gen = req._curr_gen;
					req._ft.pba = t1_info.node.pba;
				}
			} else {
				_t2_blk.encode_to_blk(_blk);
				_update_t1_node(t1_node, t1_info);
			}
			_generate_cache_req<Block_io::Write>(UPDATE_WRITE_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _blk);
			return;
		}
		case SUBTREE_TRAVERSED:

			_state = UPDATE_REQ_INVALID;
			_t1_info_stacks[lvl].pop();

			if (exchange_finished)
				while (!_t1_info_stacks[lvl].empty())
					_t1_info_stacks[lvl].pop();

			if (lvl == req._ft.max_lvl) {
				_mark_req_successful(progress);
				return;
			}
			progress = true;
			return;
		}
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
