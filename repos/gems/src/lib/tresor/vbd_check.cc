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


void Vbd_check_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("vbd check: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_node_states[_req_ptr->_vbd.max_lvl + 1][0] = DONE;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


bool Vbd_check_channel::_execute_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node const &node = _t1_blks.items[lvl].nodes[node_idx];
	Node_state &node_state = _node_states[lvl][node_idx];

	if (node_state == DONE)
		return false;

	Request &req { *_req_ptr };
	if (lvl == 1) {

		switch (node_state) {
		case READ_BLOCK: {

			if (!_num_remaining_leaves) {
				if (node.valid()) {
					_mark_req_failed(progress, "check for unused node");
					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): unexpectedly valid");
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

			} else if (_state == REQ_SUBMITTED) {

				_lvl_to_read = lvl - 1;
				generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _lvl_to_read == 0 ? _leaf_lvl : _blk, _generated_req_success);
				_state = REQ_GENERATED;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

			} else if (_state == READ_BLK_SUCCEEDED) {

				_state = REQ_SUBMITTED;
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

				if (!_num_remaining_leaves) {
					node_state = DONE;
					progress = true;
					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": expectedly invalid");

				} else {
					_mark_req_failed(progress, "check for valid node");
					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node,"): unexpectedly invalid");
				}

			} else if (_state == REQ_SUBMITTED) {
				_lvl_to_read = lvl - 1;
				generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _lvl_to_read == 0 ? _leaf_lvl : _blk, _generated_req_success);
				_state = REQ_GENERATED;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);

			} else if (_state == READ_BLK_SUCCEEDED) {
				_t1_blks.items[_lvl_to_read].decode_from_blk(_blk);
				for (Node_state &state : _node_states[lvl - 1])
					state = READ_BLOCK;

				_state = REQ_SUBMITTED;
				node_state = CHECK_HASH;
				progress = true;
			}
			break;
		}
		case CHECK_HASH: {

			Block blk { };
			_t1_blks.items[lvl - 1].encode_to_blk(blk);

			if (node.gen == INITIAL_GENERATION || check_hash(blk, node.hash)) {
				node_state = DONE;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, ": good hash");

				if (&node_state == &_node_states[_req_ptr->_vbd.max_lvl + 1][0]) {
					_req_ptr->_success = true;
					_state = REQ_COMPLETE;
					_req_ptr = nullptr;
				}
			} else {
				_mark_req_failed(progress, "check inner hash");
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._vbd.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): bad hash ", hash(blk));
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

	for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_vbd.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_vbd.degree; node_idx++)
			if (_execute_node(lvl, node_idx, progress))
				return;
}


void Vbd_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("vbd check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_node_states[_req_ptr->_vbd.max_lvl + 1][0] = DONE;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Vbd_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_lvl_to_read = 0;
	_node_states[_req_ptr->_vbd.max_lvl + 1][0] = DONE;
	_leaf_lvl = { };
	for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_vbd.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_vbd.degree; node_idx++)
			_node_states[lvl][node_idx] = DONE;

	_generated_req_success = false;
	_state = REQ_SUBMITTED;
	_num_remaining_leaves = _req_ptr->_vbd.num_leaves;
	_t1_blks.items[_req_ptr->_vbd.max_lvl + 1].nodes[0] = _req_ptr->_vbd.t1_node();
	_node_states[_req_ptr->_vbd.max_lvl + 1][0] = READ_BLOCK;
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
