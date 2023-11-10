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

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/ft_check.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

Ft_check_request::Ft_check_request(Module_id src_mod, Module_channel_id src_chan, Tree_root const &ft, bool &success)
:
	Module_request { src_mod, src_chan, FT_CHECK }, _ft { ft }, _success { success }
{ }


void Ft_check_channel::_execute_inner_t2_child(Tree_level_index  lvl,
                                       Tree_node_index   node_idx,
                                       bool             &progress)
{
	Request &req { *_req_ptr };
	Node_state &node_state { _t1_lvls[lvl].children_state[node_idx] };
	Type_1_node const &node { _t1_lvls[lvl].children.nodes[node_idx] };
	Type_2_level &child_lvl { _t2_lvl };

	if (node_state == READ_BLOCK) {

		if (!node.valid()) {

			if (_nr_of_leaves == 0) {

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

			_gen_prim = {
				.success = false,
				.tag = BLOCK_IO,
				.blk_nr = node.pba,
				.dropped = false };

			_lvl_to_read = lvl - 1;
			generate_req<Block_io::Read>(
				INVALID, progress, _gen_prim.blk_nr, _encoded_blk, _generated_req_success);
			_gen_prim.dropped = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

		} else if (_gen_prim.tag != BLOCK_IO ||
		           _gen_prim.blk_nr != node.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!_gen_prim.success) {

		} else {

			for (Node_state &state : child_lvl.children_state) {
				state = READ_BLOCK;
			}
			_gen_prim = { };
			node_state = CHECK_HASH;
			progress = true;
		}

	} else if (node_state == CHECK_HASH) {

		Block blk { };
		child_lvl.children.encode_to_blk(blk);

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


void Ft_check_channel::_execute_inner_t1_child(Type_1_node const &node,
                                       Type_1_level      &child_lvl,
                                       Node_state       &node_state,
                                       Tree_level_index   lvl,
                                       Tree_node_index    node_idx,
                                       bool              &progress)
{
	Request &req { *_req_ptr };
	if (node_state == READ_BLOCK) {

		if (!node.valid()) {

			if (_nr_of_leaves == 0) {

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

			_gen_prim = {
				.success = false,
				.tag = BLOCK_IO,
				.blk_nr = node.pba,
				.dropped = false };

			_lvl_to_read = lvl - 1;
			generate_req<Block_io::Read>(
				INVALID, progress, _gen_prim.blk_nr, _encoded_blk, _generated_req_success);
			_gen_prim.dropped = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

		} else if (_gen_prim.tag != BLOCK_IO ||
		           _gen_prim.blk_nr != node.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!_gen_prim.success) {

		} else {

			for (Node_state &state : child_lvl.children_state) {
				state = READ_BLOCK;
			}
			_gen_prim = { };
			node_state = CHECK_HASH;
			progress = true;
		}

	} else if (node_state == CHECK_HASH) {

		Block blk { };
		child_lvl.children.encode_to_blk(blk);

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


void Ft_check_channel::_execute_t2_node(Tree_node_index  node_idx,
                                   bool            &progress)
{
	Request &req { *_req_ptr };
	Type_2_node const &node { _t2_lvl.children.nodes[node_idx] };
	Node_state &node_state { _t2_lvl.children_state[node_idx] };

	if (node_state == READ_BLOCK) {

		if (_nr_of_leaves == 0) {

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

			_nr_of_leaves--;
			node_state = DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { 1, req._ft.max_lvl },
				    "    lvl 1 node ", node_idx, " done");

		}
	}
}


void Ft_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	for (Tree_node_index node_idx { 0 };
	     node_idx < req._ft.degree;
	     node_idx++) {

		if (_t2_lvl.children_state[node_idx] != DONE) {

			_execute_t2_node(node_idx, progress);
			return;
		}
	}
	for (Tree_level_index lvl { 2 }; lvl <= req._ft.max_lvl; lvl++) {

		for (Tree_node_index node_idx { 0 };
		     node_idx < req._ft.degree;
		     node_idx++) {

			Type_1_level &t1_lvl { _t1_lvls[lvl] };
			if (t1_lvl.children_state[node_idx] != DONE) {

				if (lvl == 2)
					_execute_inner_t2_child(
						lvl, node_idx, progress);
				else
					_execute_inner_t1_child(
						_t1_lvls[lvl].children.nodes[node_idx],
						_t1_lvls[lvl - 1],
						_t1_lvls[lvl].children_state[node_idx],
						lvl, node_idx, progress);

				return;
			}
		}
	}
	if (_root_state != DONE) {

		_execute_inner_t1_child(
			req._ft.t1_node(), _t1_lvls[req._ft.max_lvl], _root_state,
			req._ft.max_lvl + 1, 0, progress);

		return;
	}
	_req_ptr->_success = true;
	_req_ptr = nullptr;
}


void Ft_check_channel::_generated_req_completed(State_uint)
{
	if (!_generated_req_success) {
		error("ft check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_root_state = DONE;
		_req_ptr = nullptr;
		return;
	}
	_gen_prim.success = true;
	if (_gen_prim.tag == BLOCK_IO) {
		if (_lvl_to_read == 1)
			_t2_lvl.children.decode_from_blk(_encoded_blk);
		else
			_t1_lvls[_lvl_to_read].children.decode_from_blk(_encoded_blk);
	}
}


void Ft_check_channel::_mark_req_failed(bool       &progress,
                                char const *str)
{
	error("ft check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_root_state = DONE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_gen_prim = { };
	_lvl_to_read = 0;
	_t2_lvl = { };
	for (Type_1_level &t1_lvl : _t1_lvls)
		t1_lvl = { };
	_encoded_blk = { };
	_generated_req_success = false;
	_nr_of_leaves = _req_ptr->_ft.num_leaves;
	_root_state = READ_BLOCK;
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
