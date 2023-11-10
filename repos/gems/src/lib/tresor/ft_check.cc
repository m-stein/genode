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
	bool &check_node { _check_node[lvl][node_idx] };

	if (check_node == false)
		return false;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		if (lvl == 1) {
			Type_2_node const &node { _t2_blk.nodes[node_idx] };
			if (!_num_remaining_leaves) {
				if (node.valid()) {
					if (VERBOSE_CHECK)
						log(Level_indent { 1, req._ft.max_lvl }, "    lvl 1 node ", node_idx, " unexpectedly in use");
					_mark_req_failed(progress, "check for unused node");
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { 1, req._ft.max_lvl }, "    lvl 1 node ", node_idx, " unused");
				break;
			}
			_num_remaining_leaves--;
			check_node = false;
			progress = true;
			if (VERBOSE_CHECK)
				log(Level_indent { 1, req._ft.max_lvl }, "    lvl 1 node ", node_idx, " done");
		}
		if (lvl == 2) {
			Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
			if (!node.valid()) {
				if (_num_remaining_leaves) {
					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unexpectedly in use");
					_mark_req_failed(progress, "check for valid node");
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unused");
				break;
			}
			_lvl_to_read = lvl - 1;
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _blk);
			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);
		}
		if (lvl > 2) {
			Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
			if (!node.valid()) {
				if (_num_remaining_leaves) {
					if (VERBOSE_CHECK)
						log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unexpectedly in use");
					_mark_req_failed(progress, "check for valid node");
					break;
				}
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " unused");
				break;
			}
			_lvl_to_read = lvl - 1;
			_generate_req<Block_io::Read>(READ_BLK_SUCCEEDED, progress, node.pba, _blk);
			if (VERBOSE_CHECK)
				log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " (", node, "): load to lvl ", lvl - 1);
		}
		break;

	case READ_BLK_SUCCEEDED:

		if (_lvl_to_read == 1)
			_t2_blk.decode_from_blk(_blk);
		else
			_t1_blks.items[_lvl_to_read].decode_from_blk(_blk);

		if (lvl == 2) {

			Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
			for (bool &cn : _check_node[lvl - 1])
				cn = true;

			_t2_blk.encode_to_blk(_blk);
			if (node.gen == INITIAL_GENERATION || check_hash(_blk, node.hash)) {
				_state = REQ_SUBMITTED;
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " has good hash");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " has bad hash");
				_mark_req_failed(progress, "check inner hash");
			}
		} else if (lvl > 2) {

			Type_1_node const &node { _t1_blks.items[lvl].nodes[node_idx] };
			for (bool &cn : _check_node[lvl - 1])
				cn = true;

			_state = REQ_SUBMITTED;
			_t1_blks.items[lvl - 1].encode_to_blk(_blk);
			if (node.gen == INITIAL_GENERATION || check_hash(_blk, node.hash)) {
				check_node = false;
				progress = true;
				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " has good hash");

			} else {

				if (VERBOSE_CHECK)
					log(Level_indent { lvl, req._ft.max_lvl }, "    lvl ", lvl, " node ", node_idx, " has bad hash");
				_mark_req_failed(progress, "check inner hash");
			}
		}
		break;

	default: break;
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
		_check_node[_req_ptr->_ft.max_lvl + 1][0] = false;
		_state = REQ_COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Ft_check_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("ft check: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_check_node[_req_ptr->_ft.max_lvl + 1][0] = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_check_node[_req_ptr->_ft.max_lvl + 1][0] = false;
	_state = REQ_COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_check_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_lvl_to_read = 0;
	for (Tree_level_index lvl { 1 }; lvl <= _req_ptr->_ft.max_lvl + 1; lvl++)
		for (Tree_node_index node_idx { 0 }; node_idx < _req_ptr->_ft.degree; node_idx++)
			_check_node[lvl][node_idx] = false;
	_blk = { };
	_generated_req_success = false;
	_num_remaining_leaves = _req_ptr->_ft.num_leaves;
	_t1_blks.items[_req_ptr->_ft.max_lvl + 1].nodes[0] = _req_ptr->_ft.t1_node();
	_check_node[_req_ptr->_ft.max_lvl + 1][0] = true;
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
