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
		_t1_lvls[_req_ptr->_vbd.max_lvl + 1].children_state[0] = DONE;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_gen_prim.success = true;
	if (_gen_prim.tag == BLOCK_IO)
		if (_lvl_to_read > 0)
			_t1_lvls[_lvl_to_read].children.decode_from_blk(_encoded_blk);
}


bool Vbd_check_channel::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node const &node = _t1_lvls[lvl].children.nodes[node_idx];
	Node_state &node_state = _t1_lvls[lvl].children_state[node_idx];

	if (node_state == DONE)
		return false;

	Request &req { *_req_ptr };
	if (lvl == VBD_LOWEST_T1_LVL) {

		switch (node_state) {
		case READ_BLOCK: {

			if (_num_remaining_leaves == 0) {

				if (node.valid()) {

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): unexpectedly valid");

					_mark_req_failed(progress, "check for unused node");

				} else {

					node_state = DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": expectedly invalid");
				}

			} else if (node.gen == INITIAL_GENERATION) {

				_num_remaining_leaves--;
				node_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": uninitialized");

			} else if (!_gen_prim.valid()) {

				_gen_prim = {
					.success = false,
					.tag = BLOCK_IO,
					.blk_nr = node.pba,
					.dropped = false };

				_lvl_to_read = lvl - 1;
				generate_req<Block_io::Read>(
					INVALID, progress, node.pba, _lvl_to_read == 0 ? _leaf_lvl : _encoded_blk, _generated_req_success);
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

			} else if (_gen_prim.success) {

				_gen_prim = { };
				node_state = CHECK_HASH;
				progress = true;
			}
			break;
		}
		case CHECK_HASH: {

			if (check_hash(_leaf_lvl, node.hash)) {
				_num_remaining_leaves--;
				node_state = DONE;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": good hash");
			} else {
				_mark_req_failed(progress, "check leaf hash");
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): bad hash ", hash(_leaf_lvl));
			}
			break;
		}
		default: break;
		}

	} else {

		switch (node_state) {
		case READ_BLOCK: {

			if (!node.valid()) {

				if (_num_remaining_leaves == 0) {

					node_state = DONE;
					progress = true;

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl },
							"    lvl ", lvl, " node ", node_idx,
							": expectedly invalid");

				} else {

					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl },
							"    lvl ", lvl, " node ", node_idx, " (", node,"): unexpectedly invalid");

					_mark_req_failed(progress, "check for valid node");
				}

			} else if (!_gen_prim.valid()) {

				_gen_prim = {
					.success = false,
					.tag = BLOCK_IO,
					.blk_nr = node.pba,
					.dropped = false };

				_lvl_to_read = lvl - 1;
				generate_req<Block_io::Read>(INVALID, progress, node.pba, _lvl_to_read == 0 ? _leaf_lvl : _encoded_blk, _generated_req_success);
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

			} else if (_gen_prim.success) {

				for (Node_state &state : _t1_lvls[lvl - 1].children_state)
					state = READ_BLOCK;

				_gen_prim = { };
				node_state = CHECK_HASH;
				progress = true;
			}
			break;
		}
		case CHECK_HASH: {

			Block blk { };
			_t1_lvls[lvl - 1].children.encode_to_blk(blk);

			if (node.gen == INITIAL_GENERATION ||
				check_hash(blk, node.hash)) {

				node_state = DONE;
				progress = true;

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
						"    lvl ", lvl, " node ", node_idx, ": good hash");

				if (&node_state == &_t1_lvls[_req_ptr->_vbd.max_lvl + 1].children_state[0]) {
					_req_ptr->_success = true;
					_state = REQ_COMPLETE;
					_req_ptr = nullptr;
				}

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl },
						"    lvl ", lvl, " node ", node_idx, " (", node, "): bad hash ", hash(blk));

				_mark_req_failed(progress, "check inner hash");
			}
			break;
		}
		default: break;
		}
	}
	return true;
}


void Vbd_check_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	for (Tree_level_index lvl { VBD_LOWEST_T1_LVL }; lvl <= req._vbd.max_lvl; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < req._vbd.degree; node_idx++)
			if (_execute_node(lvl, node_idx, progress))
				return;

	if (_execute_node(req._vbd.max_lvl + 1, 0, progress))
		return;
}


void Vbd_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("vbd check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_t1_lvls[_req_ptr->_vbd.max_lvl + 1].children_state[0] = DONE;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_gen_prim = { };
	_lvl_to_read = 0;
	_t1_lvls[_req_ptr->_vbd.max_lvl + 1].children_state[0] = DONE;
	_leaf_lvl = { };
	_encoded_blk = { };
	for (Type_1_level &t1_lvl : _t1_lvls)
		t1_lvl = { };
	_generated_req_success = false;
	_state = REQ_SUBMITTED;
	_num_remaining_leaves = _req_ptr->_vbd.num_leaves;
	_t1_lvls[_req_ptr->_vbd.max_lvl + 1].children.nodes[0] = _req_ptr->_vbd.t1_node();
	_t1_lvls[_req_ptr->_vbd.max_lvl + 1].children_state[0] = READ_BLOCK;
}


Vbd_check::Vbd_check()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Vbd_check::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
