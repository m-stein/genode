/*
 * \brief  Module for initializing the VBD
 * \author Josef Soentgen
 * \date   2023-03-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/hash.h>
#include <tresor/vbd_initializer.h>

using namespace Tresor;

Vbd_initializer_request::Vbd_initializer_request(Module_id src_mod, Module_channel_id src_chan, Tree_root &vbd,
                                                 Pba_allocator &pba_alloc, bool &success)
:
	Module_request { src_mod, src_chan, VBD_INITIALIZER }, _vbd { vbd }, _pba_alloc { pba_alloc }, _success { success }
{ }


void Vbd_initializer_channel::_execute_leaf_child(bool &progress,
                                          uint64_t                             &nr_of_leaves,
                                          Type_1_node                          &child,
                                          Node_state &child_state,
                                          uint64_t                              level_index,
                                          uint64_t                              child_index)
{
	switch (child_state) {
	case INIT_BLOCK:
		child_state = INIT_NODE;
		progress = true;
		return;

	case INIT_NODE:
		if (nr_of_leaves == 0) {

			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " assign pba 0, leaf unused");

			child = { };
			child_state = DONE;
			progress = true;
		} else {

			switch (_state) {
			case IN_PROGRESS:

				child = { };
				if (!_req_ptr->_pba_alloc.alloc(child.pba)) {
					_mark_req_failed(progress, "allocate pba");
					break;
				}
				child_state = DONE;
				--nr_of_leaves;
				progress = true;

				if (VERBOSE_VBD_INIT)
					log("[vbd_init] node: ", level_index, " ", child_index,
					    " assign pba: ", child.pba, " leaves left: ",
					    nr_of_leaves);
				break;

			default:
				break;
			}
		}
	default:
		break;
	}
}


void Vbd_initializer_channel::_execute_inner_t1_child(bool &progress,
                                              uint64_t                               nr_of_leaves,
                                              uint64_t                              &level_to_write,
                                              Type_1_node                           &child,
                                              Type_1_level &child_level,
                                              Node_state  &child_state,
                                              uint64_t                               level_index,
                                              uint64_t                               child_index)

{
	switch (child_state) {
	case INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			child = { };
			child_state = DONE;
			progress = true;
			return;
		} else {

			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			_reset_level(child_level, INIT_BLOCK);
			child_state = INIT_NODE;
			progress = true;
			return;
		}
		break;

	case INIT_NODE:

		switch (_state) {
		case IN_PROGRESS:
		{
			child = { };
			if (!_req_ptr->_pba_alloc.alloc(child.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, child.hash);

			child_state = WRITE_BLOCK;
			progress = true;

			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " assign pba: ", child.pba);
			break;
		}
		default:
			break;
		}
		break;

	case WRITE_BLOCK:

		switch (_state) {
		case IN_PROGRESS:

			level_to_write = level_index - 1;
			_t1_levels[_level_to_write].children.encode_to_blk(_blk);
			generate_req<Block_io::Write>(BLOCK_IO_COMPLETE, progress, child.pba, _blk, _generated_req_success);
			_state = REQ_GENERATED;
			break;

		case BLOCK_IO_COMPLETE:

			_state = IN_PROGRESS;
			child_state = DONE;
			progress = true;
			if (VERBOSE_VBD_INIT)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " write pba: ", child.pba, " level: ",
				    level_index -1, " (child: ", child, ")");
			break;

		default: break;
		}
		break;

	default: break;
	}
}


void Vbd_initializer_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("vbd initializer: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		//_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Vbd_initializer_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Vbd_initializer_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._vbd.t1_node(_root_node.node);
	req._success = true;
	_state = COMPLETE;
	progress = true;
}


void Vbd_initializer_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = SUBMITTED;
}


void Vbd_initializer_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		for (unsigned int i = 0; i < TREE_MAX_LEVEL; i++)
			_reset_level(_t1_levels[i], Vbd_initializer_channel::DONE);
		_level_to_write = 0;
		_num_remaining_leaves = req._vbd.num_leaves;
		_state = PENDING;
		_root_node.state = Vbd_initializer_channel::INIT_BLOCK;
		progress = true;
		break;

	case PENDING:

		_state = IN_PROGRESS;
		progress = true;
		break;

	case IN_PROGRESS:
	case BLOCK_IO_COMPLETE:

		for (uint64_t level_idx = 0; level_idx <= req._vbd.max_lvl; level_idx++) {

			for (uint64_t child_idx = 0; child_idx < req._vbd.degree; child_idx++) {

				Node_state &state =
					_t1_levels[level_idx].children_state[child_idx];

				if (state != Vbd_initializer_channel::DONE) {

					Type_1_node &child =
						_t1_levels[level_idx].children.nodes[child_idx];

					if (level_idx == 1) {
						_execute_leaf_child(progress, _num_remaining_leaves,
											child, state, level_idx, child_idx);
					} else {

						Type_1_level &t1_level =
							_t1_levels[level_idx - 1];

						_execute_inner_t1_child(progress,
												_num_remaining_leaves,
												_level_to_write,
												child, t1_level, state,
												level_idx, child_idx);
					}
					return;
				}
			}
		}
		if (_root_node.state != Vbd_initializer_channel::DONE) {

			Type_1_level &t1_level =
				_t1_levels[req._vbd.max_lvl];

			_execute_inner_t1_child(progress,
									_num_remaining_leaves,
									_level_to_write,
									_root_node.node, t1_level, _root_node.state,
									req._vbd.max_lvl + 1, 0);
			return;
		}

		if (_num_remaining_leaves == 0)
			_mark_req_successful(progress);
		else
			_mark_req_failed(progress, "initialize VBD");
		break;

	default: break;
	}
}


Vbd_initializer::Vbd_initializer()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}


void Vbd_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
