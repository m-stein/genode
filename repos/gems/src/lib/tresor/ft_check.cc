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
                                       Tree_node_index   child_idx,
                                       bool             &progress)
{
	Request &req { *_req_ptr };
	Child_state &child_state { _t1_lvls[lvl].children_state[child_idx] };
	Type_1_node const &child { _t1_lvls[lvl].children.nodes[child_idx] };
	Type_2_level &child_lvl { _t2_lvl };

	if (child_state == READ_BLOCK) {

		if (!child.valid()) {

			if (_nr_of_leaves == 0) {

				child_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " unused");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " unexpectedly in use");

				_mark_req_failed(progress, "check for valid child");
			}

		} else if (!_gen_prim.valid()) {

			_gen_prim = {
				.success = false,
				.tag = BLOCK_IO,
				.blk_nr = child.pba,
				.dropped = false };

			_lvl_to_read = lvl - 1;
			generate_req<Block_io::Read>(
				INVALID, progress, _gen_prim.blk_nr, _encoded_blk, _generated_req_success);
			_gen_prim.dropped = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): load to lvl ", lvl - 1);

		} else if (_gen_prim.tag != BLOCK_IO ||
		           _gen_prim.blk_nr != child.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!_gen_prim.success) {

		} else {

			for (Child_state &state : child_lvl.children_state) {
				state = READ_BLOCK;
			}
			_gen_prim = { };
			child_state = CHECK_HASH;
			progress = true;
		}

	} else if (child_state == CHECK_HASH) {

		Block blk { };
		child_lvl.children.encode_to_blk(blk);

		if (child.gen == INITIAL_GENERATION ||
		    check_hash(blk, child.hash)) {

			child_state = DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " has good hash");

		} else {

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " has bad hash");

			_mark_req_failed(progress, "check inner hash");
		}
	}
}


void Ft_check_channel::_execute_inner_t1_child(Type_1_node const &child,
                                       Type_1_level      &child_lvl,
                                       Child_state       &child_state,
                                       Tree_level_index   lvl,
                                       Tree_node_index    child_idx,
                                       bool              &progress)
{
	Request &req { *_req_ptr };
	if (child_state == READ_BLOCK) {

		if (!child.valid()) {

			if (_nr_of_leaves == 0) {

				child_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " unused");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " unexpectedly in use");

				_mark_req_failed(progress, "check for valid child");
			}

		} else if (!_gen_prim.valid()) {

			_gen_prim = {
				.success = false,
				.tag = BLOCK_IO,
				.blk_nr = child.pba,
				.dropped = false };

			_lvl_to_read = lvl - 1;
			generate_req<Block_io::Read>(
				INVALID, progress, _gen_prim.blk_nr, _encoded_blk, _generated_req_success);
			_gen_prim.dropped = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): load to lvl ", lvl - 1);

		} else if (_gen_prim.tag != BLOCK_IO ||
		           _gen_prim.blk_nr != child.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!_gen_prim.success) {

		} else {

			for (Child_state &state : child_lvl.children_state) {
				state = READ_BLOCK;
			}
			_gen_prim = { };
			child_state = CHECK_HASH;
			progress = true;
		}

	} else if (child_state == CHECK_HASH) {

		Block blk { };
		child_lvl.children.encode_to_blk(blk);

		if (child.gen == INITIAL_GENERATION ||
		    check_hash(blk, child.hash)) {

			child_state = DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " has good hash");

		} else {

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " has bad hash");

			_mark_req_failed(progress, "check inner hash");
		}
	}
}


void Ft_check_channel::_execute_leaf_child(Tree_node_index  child_idx,
                                   bool            &progress)
{
	Request &req { *_req_ptr };
	Type_2_node const &child { _t2_lvl.children.nodes[child_idx] };
	Child_state &child_state { _t2_lvl.children_state[child_idx] };

	if (child_state == READ_BLOCK) {

		if (_nr_of_leaves == 0) {

			if (child.valid()) {

				if (VERBOSE_CHECK)
					log(Level_indent { 1, req._ft.max_lvl },
					    "    lvl 1 child ", child_idx, " unexpectedly in use");

				_mark_req_failed(progress, "check for unused child");

			} else {

				child_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { 1, req._ft.max_lvl },
					    "    lvl 1 child ", child_idx, " unused");
			}

		} else {

			_nr_of_leaves--;
			child_state = DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { 1, req._ft.max_lvl },
				    "    lvl 1 child ", child_idx, " done");

		}
	}
}


void Ft_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	for (Tree_node_index child_idx { 0 };
	     child_idx < req._ft.degree;
	     child_idx++) {

		if (_t2_lvl.children_state[child_idx] != DONE) {

			_execute_leaf_child(child_idx, progress);
			return;
		}
	}
	for (Tree_level_index lvl { FT_LOWEST_T1_LVL }; lvl <= req._ft.max_lvl; lvl++) {

		for (Tree_node_index child_idx { 0 };
		     child_idx < req._ft.degree;
		     child_idx++) {

			Type_1_level &t1_lvl { _t1_lvls[lvl] };
			if (t1_lvl.children_state[child_idx] != DONE) {

				if (lvl == FT_LOWEST_T1_LVL)
					_execute_inner_t2_child(
						lvl, child_idx, progress);
				else
					_execute_inner_t1_child(
						_t1_lvls[lvl].children.nodes[child_idx],
						_t1_lvls[lvl - 1],
						_t1_lvls[lvl].children_state[child_idx],
						lvl, child_idx, progress);

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
