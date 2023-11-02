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


void Ft_initializer_channel::_execute_leaf_child(bool                                &progress,
                                          uint64_t                            &nr_of_leaves,
                                          Type_2_node                         &child,
                                          Ft_initializer_channel::Child_state &child_state,
                                          uint64_t                             child_index)
{
	switch (child_state) {
	case INIT_BLOCK:
		child_state = INIT_NODE;
		progress = true;
		return;

	case INIT_NODE:
		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", 1, " ", child_index,
				    " assign pba 0, leaf unused");

			Ft_initializer_channel::reset_node(child);
			child_state = DONE;
			progress = true;
		} else {

			switch (_state) {
			case IN_PROGRESS:

				Ft_initializer_channel::reset_node(child);
				if (!_req_ptr->_pba_alloc.alloc(child.pba)) {
					_mark_req_failed(progress, "allocate pba");
					break;
				}
				child_state = DONE;
				--nr_of_leaves;
				progress = true;

				if (DEBUG)
					log("[ft_init] node: ", 1, " ", child_index,
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


void Ft_initializer_channel::_execute_inner_t2_child(bool                                 &progress,
                                              uint64_t                              nr_of_leaves,
                                              uint64_t                             &level_to_write,
                                              Type_1_node                          &child,
                                              Ft_initializer_channel::Type_2_level &child_level,
                                              Ft_initializer_channel::Child_state  &child_state,
                                              uint64_t                              level_index,
                                              uint64_t                              child_index)

{
	switch (child_state) {
	case INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Ft_initializer_channel::reset_node(child);
			child_state = DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Ft_initializer_channel::reset_level(child_level, INIT_BLOCK);
			child_state = INIT_NODE;
			progress = true;
			return;
		}
		break;

	case INIT_NODE:

		switch (_state) {
		case IN_PROGRESS:
		{
			Ft_initializer_channel::reset_node(child);
			if (!_req_ptr->_pba_alloc.alloc(child.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, child.hash);

			child_state = WRITE_BLOCK;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
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

			_child_pba = child.pba;
			level_to_write = level_index - 1;
			_generate_blk_io_write(progress);
			break;

		case BLOCK_IO_COMPLETE:

			_state = IN_PROGRESS;
			child_state = DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index, " write pba: ", _child_pba, " level: ",
				    level_index -1, " (child: ", child, ")");
			break;

		default: break;
		}
		break;

	default: break;
	}
}


void Ft_initializer_channel::_execute_inner_t1_child(bool                                 &progress,
                                              uint64_t                              nr_of_leaves,
                                              uint64_t                             &level_to_write,
                                              Type_1_node                          &child,
                                              Ft_initializer_channel::Type_1_level &child_level,
                                              Ft_initializer_channel::Child_state  &child_state,
                                              uint64_t                              level_index,
                                              uint64_t                              child_index)

{
	switch (child_state) {
	case INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Ft_initializer_channel::reset_node(child);
			child_state = DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Ft_initializer_channel::reset_level(child_level, INIT_BLOCK);
			child_state = INIT_NODE;
			progress = true;
			return;
		}
		break;

	case INIT_NODE:

		switch (_state) {
		case IN_PROGRESS:
		{
			Ft_initializer_channel::reset_node(child);
			if (!_req_ptr->_pba_alloc.alloc(child.pba)) {
				_mark_req_failed(progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, child.hash);

			child_state = WRITE_BLOCK;
			progress = true;

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
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

			_child_pba = child.pba;
			level_to_write = level_index - 1;
			_generate_blk_io_write(progress);
			break;

		case BLOCK_IO_COMPLETE:

			_state = IN_PROGRESS;
			child_state = DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index, " write pba: ", _child_pba, " level: ",
				    level_index -1, " (child: ", child, ")");
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
		//_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Ft_initializer_channel::_execute(bool    &progress)
{
	Request &req { *_req_ptr };

	/*
	 * First handle all leaf child nodes that starts after
	 * triggering the inner T2 nodes below.
	 */
	for (uint64_t child_idx = 0; child_idx < req._ft.degree; child_idx++) {

		Ft_initializer_channel::Child_state &state =
			_t2_level.children_state[child_idx];

		if (state != Ft_initializer_channel::Child_state::DONE) {

			Type_2_node &child =
				_t2_level.children.nodes[child_idx];

			_execute_leaf_child(progress, _num_remaining_leaves,
			                    child, state, child_idx);
		}
	}
	if (progress)
		return;

	/*
	 * Second handle all inner child nodes that starts after
	 * triggering the root node below.
	 */
	for (uint64_t level_idx = 1; level_idx <= req._ft.max_lvl; level_idx++) {

		for (uint64_t child_idx = 0; child_idx < req._ft.degree; child_idx++) {

			Ft_initializer_channel::Child_state &state =
				_t1_levels[level_idx].children_state[child_idx];

			if (state != Ft_initializer_channel::Child_state::DONE) {

				Type_1_node &child =
					_t1_levels[level_idx].children.nodes[child_idx];

				if (level_idx == 2) {
					Ft_initializer_channel::Type_2_level &t2_level =
						_t2_level;

					_execute_inner_t2_child(progress,
					                        _num_remaining_leaves,
					                        _level_to_write,
					                        child, t2_level, state,
					                        level_idx, child_idx);
				} else {

					Ft_initializer_channel::Type_1_level &t1_level =
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

	/*
	 * Checking the root node will trigger the initialization process as
	 * well as will finish it.
	 */
	if (_root_node.state != Ft_initializer_channel::Child_state::DONE) {

		Ft_initializer_channel::Type_1_level &t1_level =
			_t1_levels[req._ft.max_lvl];

		_execute_inner_t1_child(progress,
		                        _num_remaining_leaves,
		                        _level_to_write,
		                        _root_node.node, t1_level, _root_node.state,
		                        req._ft.max_lvl + 1, 0);
		return;
	}

	/*
	 * We will end up here when the root state is 'DONE'.
	 */
	if (_num_remaining_leaves == 0)
		_mark_req_successful(progress);
	else
		_mark_req_failed(progress, "initialize FT");
}


void Ft_initializer_channel::_execute_init(bool    &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_num_remaining_leaves = req._ft.num_leaves;

		/* clean residual state */
		for (unsigned int i = 0; i < TREE_MAX_LEVEL; i++) {
			Ft_initializer_channel::reset_level(_t1_levels[i],
			                                     Ft_initializer_channel::Child_state::DONE);
		}
		_level_to_write = 0;

		_state = PENDING;
		_root_node.state = Ft_initializer_channel::Child_state::INIT_BLOCK;
		progress = true;

		return;

	case PENDING:

		_state = IN_PROGRESS;
		progress = true;
		return;

	case IN_PROGRESS:

		_execute(progress);
		return;

	case BLOCK_IO_COMPLETE:

		_execute(progress);
		return;

	default:
		/*
		 * Omit other states related to ALLOC and IO as those
		 * are handled via Module API.
		 */
		return;
	}
}


void Ft_initializer_channel::_mark_req_failed(bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Ft_initializer_channel::_mark_req_successful(bool    &progress)
{
	Request &req { *_req_ptr };

	req._ft.t1_node(_root_node.node);
	req._success = true;

	_state = COMPLETE;
	progress = true;
}


void Ft_initializer_channel::execute(bool &progress)
{
	if (_state == INACTIVE)
		return;

	_execute_init(progress);
}


bool Ft_initializer::_peek_completed_request(uint8_t *buf_ptr,
                                             size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(Request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			Request &req { *channel._req_ptr };
			construct_at<Ft_initializer_request>(buf_ptr, req.src_module_id(), req.src_chan_id(), req._ft, req._pba_alloc, req._success);
			(*(Request*)buf_ptr).dst_chan_id(req.dst_chan_id());
			return true;
		}
	}
	return false;
}


void Ft_initializer::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


void Ft_initializer_channel::_generate_blk_io_write(bool &progress)
{
	if (_level_to_write == 1)
		_t2_level.children.encode_to_blk(_encoded_blk);
	else
		_t1_levels[_level_to_write].children.encode_to_blk(_encoded_blk);

	generate_req<Block_io::Write>(BLOCK_IO_COMPLETE, progress, _child_pba, _encoded_blk, _generated_req_success);
	_state = REQ_GENERATED;
}


Ft_initializer::Ft_initializer()
{
	register_channels(_channels, NR_OF_CHANNELS, FT_INITIALIZER);
}


bool Ft_initializer::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Ft_initializer::submit_request(Module_request &mod_req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			Request &req { *static_cast<Request*>(&mod_req) };
			req.dst_chan_id(id);
			_channels[id]._req_ptr.construct(req.src_module_id(), req.src_chan_id(), req._ft, req._pba_alloc, req._success);
			_channels[id]._req_ptr->dst_chan_id(id);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Ft_initializer::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}
