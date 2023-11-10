/*
 * \brief  Module for checking all hashes of a free tree or meta tree
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
#include <tresor/ft_check.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

Ft_check_request::Ft_check_request(Module_id src_mod, Module_channel_id src_chan, Tree_root const &ft, bool &success)
:
	Module_request { src_mod, src_chan, FT_CHECK }, _ft { ft }, _success { success }
{ }


bool Ft_check_channel::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Request &req { *_req_ptr };
	if (lvl == 1) {

		Type_2_node const &node { _t2_blk.nodes[node_idx] };
		Node_state &node_state { _node_states[lvl][node_idx] };

		if (node_state == DONE)
			return false;

		if (node_state == READ_BLOCK) {

			if (!_num_remaining_leaves) {

				if (node.valid()) {

					if (VERBOSE_CHECK)
						log(Level_indent { 1, req._ft.max_lvl },
							"    lvl 1 node ", node_idx, " unexpectedly in use");

					_mark_req_failed(progress, "check for unused node");

				} else {

					node_state = DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log(Level_indent { 1, req._ft.max_lvl },
							"    lvl 1 node ", node_idx, " unused");
				}

			} else {

				_num_remaining_leaves--;
				node_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { 1, req._ft.max_lvl },
						"    lvl 1 node ", node_idx, " done");

			}
		}

	} else if (lvl == 2) {

		Node_state &node_state { _node_states[lvl][node_idx] };
		Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };

		if (node_state == DONE)
			return false;

		if (node_state == READ_BLOCK) {

			if (!node.valid()) {

				if (!_num_remaining_leaves) {

					node_state = DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._ft.max_lvl },
							"    lvl ", lvl, " node ", node_idx, " unused");

				} else {

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._ft.max_lvl },
							"    lvl ", lvl, " node ", node_idx, " unexpectedly in use");

					_mark_req_failed(progress, "check for valid node");
				}

			} else if (!_gen_prim.valid()) {

				_gen_prim = { .tag = BLOCK_IO};

				_lvl_to_read = lvl - 1;
				generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _blk, _generated_req_success);

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

			} else if (_state == READ_BLK_SUCCEEDED) {

				for (Node_state &state : _node_states[lvl - 1]) {
					state = READ_BLOCK;
				}
				_gen_prim = { };
				node_state = CHECK_HASH;
				_state = REQ_SUBMITTED;
				progress = true;
			}

		} else if (node_state == CHECK_HASH) {

			Block blk { };
			_t2_blk.encode_to_blk(blk);

			if (node.gen == INITIAL_GENERATION ||
				check_hash(blk, node.hash)) {

				node_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " has good hash");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " has bad hash");

				_mark_req_failed(progress, "check inner hash");
			}
		}
	} else {

		Type_1_node const &node = _t1_blks.items[lvl].nodes[node_idx];
		Node_state       &node_state = _node_states[lvl][node_idx];

		if (node_state == DONE)
			return false;

		if (node_state == READ_BLOCK) {

			if (!node.valid()) {

				if (!_num_remaining_leaves) {

					node_state = DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._ft.max_lvl },
							"    lvl ", lvl, " node ", node_idx, " unused");

				} else {

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._ft.max_lvl },
							"    lvl ", lvl, " node ", node_idx, " unexpectedly in use");

					_mark_req_failed(progress, "check for valid node");
				}

			} else if (!_gen_prim.valid()) {

				_gen_prim = { .tag = BLOCK_IO};

				_lvl_to_read = lvl - 1;
				generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _blk, _generated_req_success);

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

			} else if (_state == READ_BLK_SUCCEEDED) {

				for (Node_state &state : _node_states[lvl - 1]) {
					state = READ_BLOCK;
				}
				_gen_prim = { };
				node_state = CHECK_HASH;
				_state = REQ_SUBMITTED;
				progress = true;
			}

		} else if (node_state == CHECK_HASH) {

			Block blk { };
			_t1_blks.items[lvl - 1].encode_to_blk(blk);

			if (node.gen == INITIAL_GENERATION ||
				check_hash(blk, node.hash)) {

				node_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " has good hash");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " has bad hash");

				_mark_req_failed(progress, "check inner hash");
			}
		}
	}
	return true;
}


void Ft_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_ft.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_ft.degree; node_idx++)
			if (_execute_node(lvl, node_idx, progress))
				return;

	_mark_req_successful(progress);
}


void Ft_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("ft check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_node_states[_req_ptr->_ft.max_lvl + 1][0] = DONE;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
	if (_state == READ_BLK_SUCCEEDED) {
		if (_lvl_to_read == 1)
			_t2_blk.decode_from_blk(_blk);
		else
			_t1_blks.items[_lvl_to_read].decode_from_blk(_blk);
	}
}


void Ft_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("ft check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_node_states[_req_ptr->_ft.max_lvl + 1][0] = DONE;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_node_states[_req_ptr->_ft.max_lvl + 1][0] = DONE;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_gen_prim = { };
	_lvl_to_read = 0;
	for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_ft.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_ft.degree; node_idx++)
			_node_states[lvl][node_idx] = DONE;
	_blk = { };
	_generated_req_success = false;
	_num_remaining_leaves = _req_ptr->_ft.num_leaves;
	_t1_blks.items[_req_ptr->_ft.max_lvl + 1].nodes[0] = _req_ptr->_ft.t1_node();
	_node_states[_req_ptr->_ft.max_lvl + 1][0] = READ_BLOCK;
	_state = REQ_SUBMITTED;
}


Ft_check::Ft_check()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Ft_check::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
