/*
 * \brief  Module for checking all hashes of a VBD snapshot
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
#include <tresor/vbd_check.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;

Vbd_check_request::Vbd_check_request(Module_id src_mod, Module_channel_id src_chan, Tree_root const &vbd, bool &success)
:
	Module_request { src_mod, src_chan, VBD_CHECK }, _vbd { vbd }, _success { success }
{ }


void Vbd_check_channel::_generated_req_completed(State_uint)
{
	if (!_generated_req_success) {
		error("vbd check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_root_state = DONE;
		_req_ptr = nullptr;
		return;
	}
	_gen_prim.success = true;
	if (_gen_prim.tag == BLOCK_IO)
		if (_lvl_to_read > 0)
			_t1_lvls[_lvl_to_read].children.decode_from_blk(_encoded_blk);
}


void Vbd_check_channel::_reset()
{
	_gen_prim = { };
	_lvl_to_read = 0;
	_root_state = DONE;
	_leaf_lvl = { };
	_encoded_blk = { };
	for (Type_1_level &t1_lvl : _t1_lvls)
		t1_lvl = { };
	_generated_req_success = false;
}


void Vbd_check_channel::_execute_inner_t1_child(Type_1_node const &child,
                                        Type_1_level      &child_lvl,
                                        Child_state       &child_state,
                                        Tree_level_index   lvl,
                                        Tree_node_index  child_idx,
                                        bool &progress)
{
	Request &req { *_req_ptr };
	if (child_state == READ_BLOCK) {

		if (!child.valid()) {

			if (_num_remaining_leaves == 0) {

				child_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
					    "    lvl ", lvl, " child ", child_idx,
					    ": expectedly invalid");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " (", child,"): unexpectedly invalid");

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
				INVALID, progress, _gen_prim.blk_nr,
				_lvl_to_read == 0 ? _leaf_lvl : _encoded_blk, _generated_req_success);
			_gen_prim.dropped = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
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
			if (&child_state == &_root_state) {
				_req_ptr->_success = true;
			}
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, ": good hash");

			if (&child_state == &_root_state) {
				_req_ptr = nullptr;
			}

		} else {

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): bad hash ", hash(blk));

			_mark_req_failed(progress, "check inner hash");
		}
	}
}


void Vbd_check_channel::_execute_leaf_child(Type_1_node const &child,
                                    Block       const &child_lvl,
                                    Child_state       &child_state,
                                    Tree_level_index   lvl,
                                    Tree_node_index  child_idx,
                                    bool &progress)
{
	Request &req { *_req_ptr };
	if (child_state == READ_BLOCK) {

		if (_num_remaining_leaves == 0) {

			if (child.valid()) {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, " (", child, "): unexpectedly valid");

				_mark_req_failed(progress, "check for unused child");

			} else {

				child_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
					    "    lvl ", lvl, " child ", child_idx, ": expectedly invalid");
			}

		} else if (child.gen == INITIAL_GENERATION) {

			_num_remaining_leaves--;
			child_state = DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, ": uninitialized");

		} else if (!_gen_prim.valid()) {

			_gen_prim = {
				.success = false,
				.tag = BLOCK_IO,
				.blk_nr = child.pba,
				.dropped = false };

			_lvl_to_read = lvl - 1;
			generate_req<Block_io::Read>(
				INVALID, progress, _gen_prim.blk_nr,
				_lvl_to_read == 0 ? _leaf_lvl : _encoded_blk, _generated_req_success);
			_gen_prim.dropped = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): load to lvl ", lvl - 1);

		} else if (_gen_prim.tag != BLOCK_IO ||
		           _gen_prim.blk_nr != child.pba) {

			class Exception_1 { };
			throw Exception_1 { };

		} else if (!_gen_prim.success) {

		} else {

			_gen_prim = { };
			child_state = CHECK_HASH;
			progress = true;
		}

	} else if (child_state == CHECK_HASH) {

		if (check_hash(child_lvl, child.hash)) {

			_num_remaining_leaves--;
			child_state = DONE;
			progress = true;

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, ": good hash");

		} else {

			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._vbd.max_lvl },
				    "    lvl ", lvl, " child ", child_idx, " (", child, "): bad hash ", hash(child_lvl));

			_mark_req_failed(progress, "check leaf hash");
		}
	}
}


void Vbd_check_channel::_execute_check(bool &progress)
{
	Request &req { *_req_ptr };
	for (Tree_level_index lvl { VBD_LOWEST_T1_LVL }; lvl <= req._vbd.max_lvl; lvl++) {
		for (Tree_node_index child_idx { 0 }; child_idx < req._vbd.degree; child_idx++) {
			Type_1_level &t1_lvl { _t1_lvls[lvl] };
			if (t1_lvl.children_state[child_idx] != DONE) {
				if (lvl == VBD_LOWEST_T1_LVL)
					_execute_leaf_child(
						_t1_lvls[lvl].children.nodes[child_idx],
						_leaf_lvl,
						_t1_lvls[lvl].children_state[child_idx],
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
			req._vbd.t1_node(), _t1_lvls[req._vbd.max_lvl], _root_state,
			req._vbd.max_lvl + 1, 0, progress);
		return;
	}
}


void Vbd_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("vbd check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_root_state = DONE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_reset();
	_root_state = READ_BLOCK;
	_num_remaining_leaves = _req_ptr->_vbd.num_leaves;
}


void Vbd_check::execute(bool &progress)
{
	for (Channel &chan : _channels) {

		if (!chan._req_ptr)
			continue;

		chan._execute_check(progress);
	}
}
