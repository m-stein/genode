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
		state, progress, Meta_tree_request::ALLOC_PBA, _req_ptr->_mt, _req_ptr->_curr_gen, pba, _generated_req_pba,
		_generated_req_success);
}


static Virtual_block_address
vbd_node_lowest_vba(Tree_degree_log_2     vbd_degree_log_2,
                    Tree_level_index      vbd_level,
                    Virtual_block_address vbd_leaf_vba)
{
	return vbd_leaf_vba & (~(Physical_block_address)0 << ((uint32_t)vbd_degree_log_2 * (uint32_t)vbd_level));
}


static Number_of_blocks vbd_node_nr_of_vbas(Tree_degree_log_2 vbd_degree_log_2,
                                            Tree_level_index  vbd_level)
{
	return (Number_of_blocks)1 << (vbd_level * vbd_degree_log_2);
}


static Virtual_block_address
vbd_node_highest_vba(Tree_degree_log_2     vbd_degree_log_2,
                     Tree_level_index      vbd_level,
                     Virtual_block_address vbd_leaf_vba)
{
	return
		vbd_node_lowest_vba(vbd_degree_log_2, vbd_level, vbd_leaf_vba) +
		(vbd_node_nr_of_vbas(vbd_degree_log_2, vbd_level) - 1);
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
                  Number_of_blocks num_requested_blks,
                  Tree_walk_pbas &new_blocks,
                  Type_1_node_walk const &old_blocks,
                  Tree_level_index max_lvl,
                  Virtual_block_address vba,
                  Tree_degree vbd_degree,
                  Virtual_block_address vbd_highest_vba,
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
	_num_requested_blks { num_requested_blks },
	_new_blocks { new_blocks },
	_old_blocks { old_blocks },
	_max_lvl { max_lvl },
	_vba { vba },
	_vbd_degree { vbd_degree },
	_vbd_highest_vba { vbd_highest_vba },
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
		_level_n_stacks[lvl].reset();
		_level_n_nodes[lvl].decode_from_blk(_cache_block_data);
		for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
			if (_level_n_nodes[lvl].nodes[idx].pba != 0) {
				_level_n_stacks[lvl].push({
					SUBTREE_NOT_TRAVERSED, _level_n_nodes[lvl].nodes[idx], idx,
					_level_n_nodes[lvl].nodes[idx].is_volatile(_req_ptr->_curr_gen) });
			}
		}
	} else {
		_level_0_stack.reset();
		_level_0_node.decode_from_blk(_cache_block_data);
		for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
			if (_t2_node_allocable(_level_0_node.nodes[idx]))
				_level_0_stack.push({ Type_2_info::INVALID, _level_0_node.nodes[idx], idx });
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
			Type_1_info n { _level_n_stacks[_generated_req_lvl].peek_top() };
			if (check_hash(_cache_block_data, n.node.hash)) {
				n.state = SUBTREE_ROOT_BLK_READ;
				_level_n_stacks[_generated_req_lvl].update_top(n);
			} else {
				error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"node hash mismatch\"");
				_req_ptr->_success = false;
				_state = COMPLETE;
			}
			break;
		}
		case UPDATE_WRITE_BLK_SUCCEEDED:
		{
			Type_1_info n { _level_n_stacks[_generated_req_lvl].peek_top() };
			n.state = SUBTREE_TRAVERSED;
			_level_n_stacks[_generated_req_lvl].update_top(n);
			break;
		}
		case UPDATE_ALLOC_PBA_SUCCEEDED: break;
		default: ASSERT_NEVER_REACHED;
		}
		break;
	}
}

Tree_level_index Free_tree_channel::_lowest_non_empty_lvl() const
{
	if (!_level_0_stack.empty())
		return 0;

	for (Tree_level_index lvl = 1; lvl <= _req_ptr->_ft.max_lvl; lvl++)
		if (!_level_n_stacks[lvl].empty())
			return lvl;

	ASSERT_NEVER_REACHED;
}


void Free_tree_channel::_alloc(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:
	{
		_exchanged_blocks = 0;
		_needed_blocks = req._num_requested_blks;
		_found_blocks = 0;
		for (Type_1_info_stack &stack : _level_n_stacks)
			stack = { };

		for (Type_1_node_block &blk : _level_n_nodes)
			blk = { };

		_level_0_stack = { };
		_level_0_node = { };
		Type_1_node root_node { req._ft.pba, req._ft.gen, req._ft.hash };
		_level_n_stacks[req._ft.max_lvl].push({ SUBTREE_NOT_TRAVERSED, root_node, 0, root_node.is_volatile(req._curr_gen) });
		_state = SCAN_READ_BLK_SUCCEEDED;
		_vbd_degree_log_2 = log2<Tree_degree_log_2>(req._vbd_degree);
		_generate_cache_req<Block_io::Read>(
			SCAN_READ_BLK_SUCCEEDED, progress, req._ft.max_lvl, req._ft.pba, _cache_block_data);
		break;
	}
	case SCAN_READ_BLK_SUCCEEDED:
	{
		Type_1_info t1_info { _level_n_stacks[_generated_req_lvl].peek_top() };
		if (!check_hash(_cache_block_data, t1_info.node.hash)) {
			_mark_req_failed(progress, "hash mismatch");
			break;
		}
		t1_info.state = SUBTREE_ROOT_BLK_READ;
		_level_n_stacks[_generated_req_lvl].update_top(t1_info);
		_traverse_tree(progress);
		break;
	}
	default: break;
	}
}


void Free_tree_channel::_traverse_tree(bool &progress)
{
	Request &req { *_req_ptr };
	while (1) {
		Tree_level_index lvl { _lowest_non_empty_lvl() };
		if (!lvl) {

			while (!_level_0_stack.empty()) {
				if (!_type_2_leafs.full())
					_type_2_leafs.enqueue(_level_0_stack.peek_top());
				_found_blocks++;
				_level_0_stack.pop();
			}
			ASSERT(!_level_n_stacks[1].empty());
			Type_1_info t1_info { _level_n_stacks[1].peek_top() };
			t1_info.state = SUBTREE_TRAVERSED;
			_level_n_stacks[1].update_top(t1_info);

		} else {

			Type_1_info t1_info = _level_n_stacks[lvl].peek_top();
			switch (t1_info.state) {
			case SUBTREE_NOT_TRAVERSED:

				_generate_cache_req<Block_io::Read>(
					SCAN_READ_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _cache_block_data);
				return;

			case SUBTREE_ROOT_BLK_READ:

				_init_info_stack_from_blk_data(lvl - 1);
				t1_info.state = SUBTREE_TRAVERSED;
				_level_n_stacks[lvl].update_top(t1_info);
				break;

			case SUBTREE_TRAVERSED:

				if (_found_blocks >= _needed_blocks) {

					/* enough free pbas were found */
					for (Type_1_info_stack &stack : _level_n_stacks)
						stack = { };

					for (Type_1_node_block &blk : _level_n_nodes)
						blk = { };

					_level_n_stacks[req._ft.max_lvl].push(
						Type_1_info {SUBTREE_NOT_TRAVERSED, _root_node(), 0, _root_node().is_volatile(req._curr_gen) });

					_state = UPDATE_REQ_INVALID;
					progress = true;
					return;

				} else if (lvl == req._ft.max_lvl) {

					/* there are not enough free pbas in the entire free tree */
					_mark_req_failed(progress, "not enough free blocks");
					return;

				} else
					_level_n_stacks[lvl].pop();

				break;

			default: ASSERT_NEVER_REACHED;
			}
		}
	}
}


void Free_tree_channel::_exchange_type_2_leaves(Number_of_blocks &exchanged, bool &handled)
{
	Request &req { *_req_ptr };
	Number_of_blocks local_exchanged { 0 };
	handled = false;

	for (Tree_level_index lvl = 0; lvl <= req._max_lvl; lvl++) {

		if (req._new_blocks.pbas[lvl] == 0) {

			if (!_level_0_stack.empty()) {

				Type_2_info t2_info { _level_0_stack.peek_top() };
				Type_2_node &t2_node { _level_0_node.nodes[t2_info.index] };
				ASSERT(t2_node.pba == t2_info.node.pba);
				switch (req._type) {
				case Request::ALLOC_FOR_NON_RKG:

					req._new_blocks.pbas[lvl] = t2_node.pba;
					t2_node.pba       = req._old_blocks.nodes[lvl].pba;
					t2_node.alloc_gen = req._old_blocks.nodes[lvl].gen;
					t2_node.free_gen  = req._free_gen;
					t2_node.last_vba  =
						vbd_node_lowest_vba(_vbd_degree_log_2, lvl, req._vba);

					if (req._rekeying) {

						if (req._vba < req._rekeying_vba)
							t2_node.last_key_id = req._curr_key_id;
						else
							t2_node.last_key_id = req._prev_key_id;

					} else {

						t2_node.last_key_id = req._curr_key_id;
					}
					t2_node.reserved = true;
					break;

				case Request::ALLOC_FOR_RKG_CURR_GEN_BLKS:

					req._new_blocks.pbas[lvl] = t2_node.pba;

					t2_node.pba       = req._old_blocks.nodes[lvl].pba;
					t2_node.alloc_gen = req._old_blocks.nodes[lvl].gen;
					t2_node.free_gen  = req._free_gen;
					t2_node.last_vba  =
						vbd_node_lowest_vba(_vbd_degree_log_2, lvl, req._vba);

					t2_node.last_key_id = req._prev_key_id;
					t2_node.reserved = false;
					break;

				case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS:
				{
					req._new_blocks.pbas[lvl] = t2_node.pba;

					t2_node.alloc_gen = req._old_blocks.nodes[lvl].gen;
					t2_node.free_gen  = req._free_gen;

					Virtual_block_address node_highest_vba {
						vbd_node_highest_vba(_vbd_degree_log_2, lvl, req._vba) };

					if (req._rekeying_vba < node_highest_vba &&
					    req._rekeying_vba < req._vbd_highest_vba)
					{
						t2_node.last_key_id = req._prev_key_id;
						t2_node.last_vba    = req._rekeying_vba + 1;

					} else if (req._rekeying_vba == node_highest_vba ||
					           req._rekeying_vba == req._vbd_highest_vba) {

						t2_node.last_key_id = req._curr_key_id;
						t2_node.last_vba = vbd_node_lowest_vba (_vbd_degree_log_2, lvl, req._vba);
					} else
						ASSERT_NEVER_REACHED;
					t2_node.reserved = true;
					break;
				}
				default: ASSERT_NEVER_REACHED;
				}
				local_exchanged = local_exchanged + 1;
				_level_0_stack.pop();
				handled = true;
			} else
				break;
		}
	}
	exchanged = local_exchanged;
}


void Free_tree_channel::_update_upper_n_stack(Type_1_info const &t,
                                      Generation         gen,
                                      Block       const &block_data,
                                      Type_1_node_block &entries)
{
	entries.nodes[t.index].pba = t.node.pba;
	entries.nodes[t.index].gen = gen;
	calc_hash(block_data, entries.nodes[t.index].hash);
}


void Free_tree_channel::_execute_update(bool &progress)
{
	Request &req { *_req_ptr };
	bool exchange_finished { false };
	bool update_finished { false };
	Number_of_blocks exchanged;

	/* handle level 0 */
	{
		bool handled;
		_exchange_type_2_leaves(exchanged, handled);
		if (handled) {
			if (exchanged > 0) {
				_exchanged_blocks += exchanged;
			} else {
				Type_1_info n { _level_n_stacks[FIRST_LVL_N_STACKS_IDX].peek_top() };
				n.state = SUBTREE_TRAVERSED;
				_level_n_stacks[FIRST_LVL_N_STACKS_IDX].update_top(n);
			}
		}
	}
	if (_exchanged_blocks == _needed_blocks) {
		exchange_finished = true;
	}
	/* handle level 1..N */
	for (Tree_level_index lvl { FIRST_LVL_N_STACKS_IDX }; lvl <= MAX_LVL_N_STACKS_IDX; lvl++) {

		Type_1_info_stack &stack { _level_n_stacks[lvl] };
		if (!stack.empty()) {

			Type_1_info t1_info { stack.peek_top() };
			switch (t1_info.state) {
			case SUBTREE_NOT_TRAVERSED:

				ASSERT(_state == UPDATE_REQ_INVALID);
				_generate_cache_req<Block_io::Read>(
					UPDATE_READ_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _cache_block_data);
				break;

			case SUBTREE_ROOT_BLK_READ:

				_state = UPDATE_REQ_INVALID;
				_init_info_stack_from_blk_data(lvl - 1);
				if (lvl > 1) {
					if (!_level_n_stacks[lvl - 1].empty())
						t1_info.state = X_WRITE;
					else
						t1_info.state = SUBTREE_TRAVERSED;
				} else {
					if (!_level_0_stack.empty())
						t1_info.state = X_WRITE;
					else
						t1_info.state = SUBTREE_TRAVERSED;
				}
				stack.update_top(t1_info);
				progress = true;
				break;

			case X_WRITE:

				if (!t1_info.volatil) {

					if (_state == UPDATE_REQ_INVALID) {

						_generate_mt_req(UPDATE_ALLOC_PBA_SUCCEEDED, progress, t1_info.node.pba);
						break;

					} else if (_state == UPDATE_ALLOC_PBA_SUCCEEDED) {

						_state = UPDATE_REQ_INVALID;
						t1_info.volatil = true;
						t1_info.node.pba = _generated_req_pba;
						stack.update_top(t1_info);

					} else {

						class Exception_3 { };
						throw Exception_3 { };
					}
				}
				if (lvl >= 2) {

					_level_n_nodes[lvl - 1].encode_to_blk(_cache_block_data);

					if (lvl < req._ft.max_lvl) {
						_update_upper_n_stack(
							t1_info, req._curr_gen, _cache_block_data,
							_level_n_nodes[lvl]);
					} else {
						calc_hash(_cache_block_data,
						                    req._ft.hash);

						req._ft.gen = req._curr_gen;
						req._ft.pba =
							t1_info.node.pba;
					}
				} else {
					_level_0_node.encode_to_blk(_cache_block_data);

					_update_upper_n_stack(
						t1_info, req._curr_gen, _cache_block_data,
						_level_n_nodes[lvl]);
				}
				_generate_cache_req<Block_io::Write>(
					UPDATE_WRITE_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _cache_block_data);
				break;

			case SUBTREE_TRAVERSED:

				_state = UPDATE_REQ_INVALID;
				stack.pop();

				if (exchange_finished)
					while (!stack.empty())
						stack.pop();

				if (lvl == req._ft.max_lvl)
					update_finished = true;

				progress = true;
				break;
			}
			break;
		}
	}
	switch (_state) {
	case UPDATE_REQ_INVALID:
	case UPDATE_REQ_GENERATED:
	case UPDATE_READ_BLK_SUCCEEDED:
	case UPDATE_ALLOC_PBA_SUCCEEDED:
	case UPDATE_WRITE_BLK_SUCCEEDED: break;
	default: return;
	}

	if (exchange_finished && update_finished)
		_mark_req_successful(progress);
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

	if (_state == UPDATE_REQ_GENERATED)
		return;

	switch (_state) {
	case REQ_SUBMITTED:
	case SCAN_READ_BLK_SUCCEEDED: _alloc(progress); break;
	case UPDATE_READ_BLK_SUCCEEDED:
	case UPDATE_WRITE_BLK_SUCCEEDED:
	case UPDATE_ALLOC_PBA_SUCCEEDED:
	case UPDATE_REQ_INVALID: _execute_update(progress); break;
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
