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
	ASSERT(_generated_req_state == REQ_INVALID);
	_generated_req_state = REQ_IN_PROGRESS;
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
		if (!chan._req_ptr)
			return;
		_execute(chan, chan._req_ptr->_snapshots, chan._req_ptr->_last_secured_gen, progress);
	});
}




void Free_tree_channel::_populate_lower_n_stack(Type_1_info_stack &stack,
                                        Type_1_node_block &entries,
                                        Block      const  &block_data,
                                        Generation         current_gen)
{
	stack.reset();
	entries.decode_from_blk(block_data);

	for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {

		if (entries.nodes[idx].pba != 0) {

			stack.push({
				Type_1_info::INVALID, entries.nodes[idx], idx,
				entries.nodes[idx].is_volatile(current_gen) });
		}
	}
}


bool
Free_tree_channel::_check_type_2_leaf_usable(Snapshots       const &snapshots,
                                     Generation             last_secured_gen,
                                     Type_2_node     const &node,
                                     bool                   rekeying,
                                     Key_id                 prev_key_id,
                                     Virtual_block_address  rekeying_vba)
{
	if (node.pba == 0 ||
	    node.pba == INVALID_PBA ||
	    node.free_gen > last_secured_gen)
		return false;

	if (!node.reserved)
		return true;

	if (rekeying &&
	    node.last_key_id == prev_key_id &&
	    node.last_vba < rekeying_vba)
		return true;

	for (Snapshot const &snap : snapshots.items) {
		if (snap.valid &&
		    node.free_gen > snap.gen &&
		    node.alloc_gen < snap.gen + 1)
			return false;
	}
	return true;
}


void Free_tree_channel::_populate_level_0_stack(Type_2_info_stack     &stack,
                                        Type_2_node_block     &entries,
                                        Block           const &block_data,
                                        Snapshots       const &active_snaps,
                                        Generation             secured_gen,
                                        bool                   rekeying,
                                        Key_id                 prev_key_id,
                                        Virtual_block_address  rekeying_vba)
{
	stack.reset();
	entries.decode_from_blk(block_data);

	for (Tree_node_index idx = 0; idx < NR_OF_T1_NODES_PER_BLK; idx++) {
		if (_check_type_2_leaf_usable(active_snaps, secured_gen,
		                              entries.nodes[idx], rekeying,
		                              prev_key_id, rekeying_vba)) {

			stack.push({ Type_2_info::INVALID, entries.nodes[idx], idx });
		}
	}
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
		switch (_state) {
		case SCAN_READ_BLK_SUCCEEDED:
		{
			Type_1_info n { _level_n_stacks[_generated_req_lvl].peek_top() };
			if (check_hash(_cache_block_data, n.node.hash)) {
				n.state = Type_1_info::AVAILABLE;
				_level_n_stacks[_generated_req_lvl].update_top(n);
			} else {
				error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"node hash mismatch\"");
				_req_ptr->_success = false;
				_state = COMPLETE;
			}
			break;
		}
		default: ASSERT_NEVER_REACHED;
		}
		break;
	default:
		ASSERT(_generated_req_state == REQ_IN_PROGRESS);
		if (!_generated_req_success) {
			error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
			_req_ptr->_success = false;
			_state = COMPLETE;
			return;
		}
		_generated_req_state = (Gen_req_state)state_uint;
		switch (_generated_req_state) {
		case READ_COMPLETE:
		{
			Type_1_info n { _level_n_stacks[_generated_req_lvl].peek_top() };
			if (check_hash(_cache_block_data, n.node.hash)) {
				n.state = Type_1_info::AVAILABLE;
				_level_n_stacks[_generated_req_lvl].update_top(n);
			} else {
				error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"node hash mismatch\"");
				_req_ptr->_success = false;
				_state = COMPLETE;
			}
			break;
		}
		case WRITE_COMPLETE:
		{
			Type_1_info n { _level_n_stacks[_generated_req_lvl].peek_top() };
			n.state = Type_1_info::COMPLETE;
			_level_n_stacks[_generated_req_lvl].update_top(n);
			break;
		}
		case ALLOC_COMPLETE: break;
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


void Free_tree::_alloc(Channel &chan,
                       Snapshots const &,
                       Generation,
                       bool &progress)
{
	Request &req { *chan._req_ptr };
	switch (chan._state) {
	case Channel::REQ_SUBMITTED:
	{
		chan._exchanged_blocks = 0;
		chan._needed_blocks = req._num_requested_blks;
		chan._found_blocks = 0;
		for (Type_1_info_stack &stack : chan._level_n_stacks)
			stack = { };

		for (Type_1_node_block &blk : chan._level_n_nodes)
			blk = { };

		chan._level_0_stack = { };
		chan._level_n_node = { };
		chan._level_0_node = { };
		Type_1_node root_node { };
		root_node.pba = req._ft.pba;
		root_node.gen = req._ft.gen;
		root_node.hash = req._ft.hash;
		chan._level_n_stacks[req._ft.max_lvl].push({ Type_1_info::INVALID, root_node, 0, root_node.is_volatile(req._curr_gen) });
		chan._state = Channel::SCAN_READ_BLK_SUCCEEDED;
		chan._vbd_degree_log_2 = log2<Tree_degree_log_2>(req._vbd_degree);
		progress = true;
		break;
	}
	case Channel::SCAN_READ_BLK_SUCCEEDED:
	{
		chan._traverse_tree(progress);
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

			/*
			 * Only when the node is in read state we actually have to acknowledge
			 * checking the leaf nodes.
			 */
			if (t1_info.state == Type_1_info::READ) {
				t1_info.state = Type_1_info::COMPLETE;
				_level_n_stacks[1].update_top(t1_info);
			}
		} else {

			Type_1_info t1_info = _level_n_stacks[lvl].peek_top();
			switch (t1_info.state) {
			case Type_1_info::INVALID:

				_generate_cache_req<Block_io::Read>(
					SCAN_READ_BLK_SUCCEEDED, progress, lvl, t1_info.node.pba, _cache_block_data);
				return;

			case Type_1_info::AVAILABLE:

				if (lvl >= 2) {
					_populate_lower_n_stack(
						_level_n_stacks[lvl - 1],
						_level_n_node, _cache_block_data,
						req._curr_gen);
				} else {
					_populate_level_0_stack(
						_level_0_stack,
						_level_0_node, _cache_block_data,
						req._snapshots, req._last_secured_gen,
						req._rekeying, req._prev_key_id,
						req._rekeying_vba);
				}
				t1_info.state = Type_1_info::READ;
				_level_n_stacks[lvl].update_top(t1_info);
				break;

			case Type_1_info::READ:

				t1_info.state = Type_1_info::COMPLETE;
				_level_n_stacks[lvl].update_top(t1_info);
				break;

			case Type_1_info::COMPLETE:

				if (_found_blocks >= _needed_blocks) {

					/* enough free pbas were found */
					for (Type_1_info_stack &stack : _level_n_stacks)
						stack = { };

					for (Type_1_node_block &blk : _level_n_nodes)
						blk = { };

					_level_n_stacks[req._ft.max_lvl].push(
						Type_1_info {
							Type_1_info::INVALID, _root_node(), 0,
							_root_node().is_volatile(req._curr_gen) });

					_state = UPDATE;
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


void
Free_tree::_exchange_type_2_leaves(Generation              free_gen,
                                   Tree_level_index        max_lvl,
                                   Type_1_node_walk const &old_blocks,
                                   Tree_walk_pbas         &new_blocks,
                                   Virtual_block_address   vba,
                                   Tree_degree_log_2       vbd_degree_log_2,
                                   Request::Type           req_type,
                                   Type_2_info_stack      &stack,
                                   Type_2_node_block      &entries,
                                   Number_of_blocks       &exchanged,
                                   bool                   &handled,
                                   Virtual_block_address   vbd_highest_vba,
                                   bool                    rekeying,
                                   Key_id                  prev_key_id,
                                   Key_id                  curr_key_id,
                                   Virtual_block_address   rekeying_vba)
{
	Number_of_blocks local_exchanged { 0 };
	handled = false;

	for (Tree_level_index i = 0; i <= max_lvl; i++) {

		if (new_blocks.pbas[i] == 0) {

			if (!stack.empty()) {

				Type_2_info const info { stack.peek_top() };
				Type_2_node &t2_node { entries.nodes[info.index] };
				if (t2_node.pba != info.node.pba) {
					class Exception_1 { };
					throw Exception_1 { };
				}
				switch (req_type) {
				case Request::ALLOC_FOR_NON_RKG:

					new_blocks.pbas[i] = t2_node.pba;
					t2_node.pba       = old_blocks.nodes[i].pba;
					t2_node.alloc_gen = old_blocks.nodes[i].gen;
					t2_node.free_gen  = free_gen;
					t2_node.last_vba  =
						vbd_node_lowest_vba(vbd_degree_log_2, i, vba);

					if (rekeying) {

						if (vba < rekeying_vba)
							t2_node.last_key_id = curr_key_id;
						else
							t2_node.last_key_id = prev_key_id;

					} else {

						t2_node.last_key_id = curr_key_id;
					}
					t2_node.reserved = true;
					break;

				case Request::ALLOC_FOR_RKG_CURR_GEN_BLKS:

					new_blocks.pbas[i] = t2_node.pba;

					t2_node.pba       = old_blocks.nodes[i].pba;
					t2_node.alloc_gen = old_blocks.nodes[i].gen;
					t2_node.free_gen  = free_gen;
					t2_node.last_vba  =
						vbd_node_lowest_vba (vbd_degree_log_2, i, vba);

					t2_node.last_key_id = prev_key_id;
					t2_node.reserved = false;
					break;

				case Request::ALLOC_FOR_RKG_OLD_GEN_BLKS:
				{
					new_blocks.pbas[i] = t2_node.pba;

					t2_node.alloc_gen = old_blocks.nodes[i].gen;
					t2_node.free_gen  = free_gen;

					Virtual_block_address const node_highest_vba {
						vbd_node_highest_vba(vbd_degree_log_2, i, vba) };

					if (rekeying_vba < node_highest_vba &&
					    rekeying_vba < vbd_highest_vba)
					{
						t2_node.last_key_id = prev_key_id;
						t2_node.last_vba    = rekeying_vba + 1;

					} else if (rekeying_vba == node_highest_vba ||
					           rekeying_vba == vbd_highest_vba) {

						t2_node.last_key_id = curr_key_id;
						t2_node.last_vba    =
							vbd_node_lowest_vba (vbd_degree_log_2, i, vba);

					} else {

						class Exception_1 { };
						throw Exception_1 { };
					}
					t2_node.reserved = true;
					break;
				}
				default:

					class Exception_2 { };
					throw Exception_2 { };
				}

				local_exchanged = local_exchanged + 1;
				stack.pop();
				handled = true;

			} else {

				break;
			}
		}
	}
	exchanged = local_exchanged;
}


void Free_tree::_update_upper_n_stack(Type_1_info const &t,
                                      Generation         gen,
                                      Block       const &block_data,
                                      Type_1_node_block &entries)
{
	entries.nodes[t.index].pba = t.node.pba;
	entries.nodes[t.index].gen = gen;
	calc_hash(block_data, entries.nodes[t.index].hash);
}


void Free_tree::_execute_update(Channel         &chan,
                                Snapshots const &active_snaps,
                                Generation       last_secured_gen,
                                bool            &progress)
{
	Request &req { *chan._req_ptr };
	bool exchange_finished { false };
	bool update_finished { false };
	Number_of_blocks exchanged;

	/* handle level 0 */
	{
		bool handled;

		_exchange_type_2_leaves(
			req._free_gen, req._max_lvl,
			req._old_blocks,
			req._new_blocks,
			req._vba, chan._vbd_degree_log_2, req._type, chan._level_0_stack,
			chan._level_0_node, exchanged, handled, req._vbd_highest_vba,
			req._rekeying, req._prev_key_id, req._curr_key_id,
			req._rekeying_vba);

		if (handled) {
			if (exchanged > 0) {
				chan._exchanged_blocks += exchanged;
			} else {
				Type_1_info n { chan._level_n_stacks[FIRST_LVL_N_STACKS_IDX].peek_top() };
				n.state = Type_1_info::COMPLETE;
				chan._level_n_stacks[FIRST_LVL_N_STACKS_IDX].update_top(n);
			}
		}
	}
	if (chan._exchanged_blocks == chan._needed_blocks) {
		exchange_finished = true;
	}
	/* handle level 1..N */
	for (Tree_level_index l { FIRST_LVL_N_STACKS_IDX }; l <= MAX_LVL_N_STACKS_IDX; l++) {

		Type_1_info_stack &stack { chan._level_n_stacks[l] };

		if (!stack.empty()) {

			Type_1_info n { stack.peek_top() };
			switch (n.state) {
			case Type_1_info::INVALID:

				ASSERT(chan._generated_req_state == Channel::REQ_INVALID);
				chan._generate_cache_req<Block_io::Read>(
					Channel::READ_COMPLETE, progress, l, n.node.pba, chan._cache_block_data);
				break;

			case Type_1_info::AVAILABLE:

				chan._generated_req_state = Channel::REQ_INVALID;
				if (l >= 2) {

					chan._populate_lower_n_stack(
						chan._level_n_stacks[l - 1],
						chan._level_n_nodes[l - 1], chan._cache_block_data,
						req._curr_gen);

					if (!chan._level_n_stacks[l - 1].empty())
						n.state = Type_1_info::WRITE;
					else
						n.state = Type_1_info::COMPLETE;

				} else {

					chan._populate_level_0_stack(
						chan._level_0_stack, chan._level_0_node,
						chan._cache_block_data, active_snaps, last_secured_gen,
						req._rekeying, req._prev_key_id,
						req._rekeying_vba);

					if (!chan._level_0_stack.empty())
						n.state = Type_1_info::WRITE;
					else
						n.state = Type_1_info::COMPLETE;
				}
				stack.update_top(n);
				progress = true;
				break;

			case Type_1_info::READ:

				class Exception_2 { };
				throw Exception_2 { };

			case Type_1_info::WRITE:

				if (!n.volatil) {

					if (chan._generated_req_state == Channel::REQ_INVALID) {

						chan._generate_mt_req(Channel::ALLOC_COMPLETE, progress, n.node.pba);
						break;

					} else if (chan._generated_req_state == Channel::ALLOC_COMPLETE) {

						chan._generated_req_state = Channel::REQ_INVALID;
						n.volatil = true;
						n.node.pba = chan._generated_req_pba;
						stack.update_top(n);

					} else {

						class Exception_3 { };
						throw Exception_3 { };
					}
				}
				if (l >= 2) {

					chan._level_n_nodes[l - 1].encode_to_blk(chan._cache_block_data);

					if (l < req._ft.max_lvl) {
						_update_upper_n_stack(
							n, req._curr_gen, chan._cache_block_data,
							chan._level_n_nodes[l]);
					} else {
						calc_hash(chan._cache_block_data,
						                    req._ft.hash);

						req._ft.gen = req._curr_gen;
						req._ft.pba =
							n.node.pba;
					}
				} else {
					chan._level_0_node.encode_to_blk(chan._cache_block_data);

					_update_upper_n_stack(
						n, req._curr_gen, chan._cache_block_data,
						chan._level_n_nodes[l]);
				}
				chan._generate_cache_req<Block_io::Write>(
					Channel::WRITE_COMPLETE, progress, l, n.node.pba, chan._cache_block_data);
				break;

			case Type_1_info::COMPLETE:

				chan._generated_req_state = Channel::REQ_INVALID;
				stack.pop();

				if (exchange_finished)
					while (!stack.empty())
						stack.pop();

				if (l == req._ft.max_lvl)
					update_finished = true;

				progress = true;
				break;
			}
			break;
		}
	}
	if (chan._state != Channel::UPDATE)
		return;

	if (exchange_finished && update_finished)
		_mark_req_successful(chan, progress);
}


void Free_tree_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"", str, "\"");
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Free_tree::_mark_req_successful(Channel &channel,
                                     bool &progress)
{
	channel._req_ptr->_success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Free_tree::_execute(Channel &chan,
                         Snapshots const &snapshots,
                         Generation last_secured_gen,
                         bool &progress)
{
	if (chan._generated_req_state == Channel::REQ_IN_PROGRESS)
		return;

	switch (chan._state) {
	case Channel::REQ_SUBMITTED:
	case Channel::SCAN_READ_BLK_SUCCEEDED:
		_alloc(chan, snapshots, last_secured_gen, progress);
		break;
	case Channel::UPDATE:
		_execute_update(chan, snapshots, last_secured_gen, progress);
		break;
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
