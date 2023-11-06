/*
 * \brief  Module for initializing the free tree
 * \author Josef Soentgen
 * \date   2023-03-09
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
#include <tresor/block_io.h>
#include <tresor/hash.h>
#include <tresor/ft_initializer.h>

using namespace Tresor;

static constexpr bool DEBUG = false;

Ft_initializer_request::Ft_initializer_request(Module_id src_mod, Module_channel_id src_chan,
                                               Free_tree_root &ft, Pba_allocator &pba_alloc, bool &success)
:
	Module_request { src_mod, src_chan, FT_INITIALIZER }, _ft { ft }, _pba_alloc { pba_alloc }, _success { success }
{ }


void Ft_initializer_channel::_execute_t2_node(Tree_node_index node_idx, bool &progress)
{
	Node_state &node_state { _t2_level.children_state[node_idx] };
	Type_2_node &node { _t2_level.children.nodes[node_idx] };
	switch (node_state) {
	case INIT_BLOCK:

		node_state = INIT_NODE;
		progress = true;
		break;

	case INIT_NODE:

		if (_num_remaining_leaves) {
			if (_state != IN_PROGRESS)
				break;

			node = { };
			if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			node_state = DONE;
			_num_remaining_leaves--;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", 1, " ", node_idx, " assign pba: ", node.pba, " leaves left: ", _num_remaining_leaves);
		} else {
			node = { };
			node_state = DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", 1, " ", node_idx, " assign pba 0, leaf unused");
		}
		break;

	default: break;
	}
}


void Ft_initializer_channel::_execute_lowest_t1_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node &node { _t1_levels[lvl].children.nodes[node_idx] };
	Node_state &node_state { _t1_levels[lvl].children_state[node_idx] };
	switch (node_state) {
	case INIT_BLOCK:

		if (_num_remaining_leaves) {
			reset_level(_t2_level, INIT_BLOCK);
			node_state = INIT_NODE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx, " reset level: ", lvl - 1);
		} else {
			node = { };
			node_state = DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx, " assign pba 0, inner node unused");
		}
		break;

	case INIT_NODE:
	{
		if (_state != IN_PROGRESS)
			break;

		node = { };
		if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
			_mark_req_failed(progress, "allocate pba");
			break;
		}
		Block blk { };
		_t2_level.children.encode_to_blk(blk);
		calc_hash(blk, node.hash);
		node_state = WRITE_BLOCK;
		progress = true;
		if (DEBUG)
			log("[ft_init] node: ", lvl, " ", node_idx, " assign pba: ", node.pba);
		break;
	}
	case WRITE_BLOCK:

		switch (_state) {
		case IN_PROGRESS:

			_pba = node.pba;
			_level_to_write = lvl - 1;
			_generate_blk_io_write(progress);
			break;

		case BLOCK_IO_COMPLETE:

			_state = IN_PROGRESS;
			node_state = DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx, " write pba: ", _pba, " level: ",
				    lvl -1, " (node: ", node, ")");
			break;

		default: break;
		}
		break;

	default: break;
	}
}


void Ft_initializer_channel::_execute_inner_t1_node(Tree_level_index lvl, Tree_node_index node_idx, bool &progress)
{
	Type_1_node &node {  _t1_levels[lvl].children.nodes[node_idx] };
	Node_state &node_state { _t1_levels[lvl].children_state[node_idx] };
	Type_1_level &child_level { _t1_levels[lvl - 1] };
	switch (node_state) {
	case INIT_BLOCK:

		if (_num_remaining_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx,
				    " assign pba 0, inner node unused");

			node = { };
			node_state = DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx,
				    " reset level: ", lvl - 1);

			reset_level(child_level, INIT_BLOCK);
			node_state = INIT_NODE;
			progress = true;
			return;
		}
		break;

	case INIT_NODE:

		switch (_state) {
		case IN_PROGRESS:
		{
			node = { };
			if (!_req_ptr->_pba_alloc.alloc(node.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, node.hash);

			node_state = WRITE_BLOCK;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx,
				    " assign pba: ", node.pba);
			break;
		}
		default:
			break;
		}
		break;

	case WRITE_BLOCK:

		switch (_state) {
		case IN_PROGRESS:

			_pba = node.pba;
			_level_to_write = lvl - 1;
			_generate_blk_io_write(progress);
			break;

		case BLOCK_IO_COMPLETE:

			_state = IN_PROGRESS;
			node_state = DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", lvl, " ", node_idx, " write pba: ", _pba, " level: ",
				    lvl - 1, " (node: ", node, ")");
			break;

		default: break;
		}
		break;

	default: break;
	}
}


void Ft_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("ft initializer: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Ft_initializer_channel::_execute(bool    &progress)
{
	Request &req { *_req_ptr };
	for (Tree_node_index node_idx = 0; node_idx < req._ft.degree; node_idx++)
		_execute_t2_node(node_idx, progress);

	if (progress)
		return;

	for (Tree_level_index lvl = 1; lvl <= req._ft.max_lvl; lvl++) {
		for (Tree_node_index node_idx = 0; node_idx < req._ft.degree; node_idx++) {
			if (lvl == 2)
				_execute_lowest_t1_node(lvl, node_idx, progress);
			else
				_execute_inner_t1_node(lvl, node_idx, progress);
			if (progress)
				return;
		}
	}
	_execute_inner_t1_node(req._ft.max_lvl + 1, 0, progress);
	if (progress)
		return;

	if (_num_remaining_leaves) {
		_mark_req_failed(progress, "initialize FT");
		return;
	}
	_mark_req_successful(progress);
}


void Ft_initializer_channel::_execute_init(bool    &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_num_remaining_leaves = req._ft.num_leaves;

		/* clean residual state */
		for (unsigned int i = 0; i < TREE_MAX_LEVEL; i++)
			reset_level(_t1_levels[i], DONE);

		_level_to_write = 0;
		_state = PENDING;
		_t1_levels[req._ft.max_lvl + 1].children_state[0] = INIT_BLOCK;
		progress = true;
		return;

	case PENDING:

		_state = IN_PROGRESS;
		progress = true;
		return;

	case IN_PROGRESS: _execute(progress); return;
	case BLOCK_IO_COMPLETE: _execute(progress); return;
	default: return;
	}
}


void Ft_initializer_channel::_mark_req_failed(bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_initializer_channel::_mark_req_successful(bool    &progress)
{
	_req_ptr->_ft.t1_node(_t1_levels[_req_ptr->_ft.max_lvl + 1].children.nodes[0]);
	_req_ptr->_success = true;
	_state = COMPLETE;
	_req_ptr = nullptr;
	progress = true;
}


void Ft_initializer_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	_execute_init(progress);
}


void Ft_initializer_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = SUBMITTED;
}


void Ft_initializer_channel::_generate_blk_io_write(bool &progress)
{
	if (_level_to_write == 1)
		_t2_level.children.encode_to_blk(_blk);
	else
		_t1_levels[_level_to_write].children.encode_to_blk(_blk);

	generate_req<Block_io::Write>(BLOCK_IO_COMPLETE, progress, _pba, _blk, _generated_req_success);
	_state = REQ_GENERATED;
}


Ft_initializer::Ft_initializer()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Ft_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
