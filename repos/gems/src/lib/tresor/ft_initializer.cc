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


void Ft_initializer::_execute_leaf_child(Channel                              &channel,
                                          bool                                &progress,
                                          uint64_t                            &nr_of_leaves,
                                          Type_2_node                         &child,
                                          Ft_initializer_channel::Child_state &child_state,
                                          uint64_t                             child_index)
{
	using CS = Ft_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:
		child_state = CS::INIT_NODE;
		progress = true;
		return;

	case CS::INIT_NODE:
		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", 1, " ", child_index,
				    " assign pba 0, leaf unused");

			Ft_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
		} else {

			switch (channel._state) {
			case Channel::IN_PROGRESS:

				Ft_initializer_channel::reset_node(child);
				if (!channel._req_ptr->_pba_alloc.alloc(child.pba)) {
					_mark_req_failed(channel, progress, "allocate pba");
					break;
				}
				child_state = CS::DONE;
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


void Ft_initializer::_execute_inner_t2_child(Channel                               &channel,
                                              bool                                 &progress,
                                              uint64_t                              nr_of_leaves,
                                              uint64_t                             &level_to_write,
                                              Type_1_node                          &child,
                                              Ft_initializer_channel::Type_2_level &child_level,
                                              Ft_initializer_channel::Child_state  &child_state,
                                              uint64_t                              level_index,
                                              uint64_t                              child_index)

{
	using CS = Ft_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Ft_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Ft_initializer_channel::reset_level(child_level, CS::INIT_BLOCK);
			child_state = CS::INIT_NODE;
			progress = true;
			return;
		}
		break;

	case CS::INIT_NODE:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
		{
			Ft_initializer_channel::reset_node(child);
			if (!channel._req_ptr->_pba_alloc.alloc(child.pba)) {
				_mark_req_failed(channel, progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, child.hash);

			child_state = CS::WRITE_BLOCK;
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

	case CS::WRITE_BLOCK:

		switch (channel._state) {
		case Channel::IN_PROGRESS:

			channel._child_pba = child.pba;
			level_to_write = level_index - 1;
			channel._generate_blk_io_write(progress);
			break;

		case Channel::BLOCK_IO_COMPLETE:

			channel._state = Channel::IN_PROGRESS;
			child_state = CS::DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index, " write pba: ", channel._child_pba, " level: ",
				    level_index -1, " (child: ", child, ")");
			break;

		default: break;
		}
		break;

	default: break;
	}
}


void Ft_initializer::_execute_inner_t1_child(Channel                               &channel,
                                              bool                                 &progress,
                                              uint64_t                              nr_of_leaves,
                                              uint64_t                             &level_to_write,
                                              Type_1_node                          &child,
                                              Ft_initializer_channel::Type_1_level &child_level,
                                              Ft_initializer_channel::Child_state  &child_state,
                                              uint64_t                              level_index,
                                              uint64_t                              child_index)

{
	using CS = Ft_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Ft_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Ft_initializer_channel::reset_level(child_level, CS::INIT_BLOCK);
			child_state = CS::INIT_NODE;
			progress = true;
			return;
		}
		break;

	case CS::INIT_NODE:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
		{
			Ft_initializer_channel::reset_node(child);
			if (!channel._req_ptr->_pba_alloc.alloc(child.pba)) {
				_mark_req_failed(channel, progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, child.hash);

			child_state = CS::WRITE_BLOCK;
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

	case CS::WRITE_BLOCK:

		switch (channel._state) {
		case Channel::IN_PROGRESS:

			channel._child_pba = child.pba;
			level_to_write = level_index - 1;
			channel._generate_blk_io_write(progress);
			break;

		case Channel::BLOCK_IO_COMPLETE:

			channel._state = Channel::IN_PROGRESS;
			child_state = CS::DONE;
			progress = true;
			if (DEBUG)
				log("[ft_init] node: ", level_index, " ", child_index, " write pba: ", channel._child_pba, " level: ",
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


void Ft_initializer::_execute(Channel &channel,
                               bool    &progress)
{
	Request &req { *channel._req_ptr };

	/*
	 * First handle all leaf child nodes that starts after
	 * triggering the inner T2 nodes below.
	 */
	for (uint64_t child_idx = 0; child_idx < req._ft.degree; child_idx++) {

		Ft_initializer_channel::Child_state &state =
			channel._t2_level.children_state[child_idx];

		if (state != Ft_initializer_channel::Child_state::DONE) {

			Type_2_node &child =
				channel._t2_level.children.nodes[child_idx];

			_execute_leaf_child(channel, progress, channel._num_remaining_leaves,
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
				channel._t1_levels[level_idx].children_state[child_idx];

			if (state != Ft_initializer_channel::Child_state::DONE) {

				Type_1_node &child =
					channel._t1_levels[level_idx].children.nodes[child_idx];

				if (level_idx == 2) {
					Ft_initializer_channel::Type_2_level &t2_level =
						channel._t2_level;

					_execute_inner_t2_child(channel, progress,
					                        channel._num_remaining_leaves,
					                        channel._level_to_write,
					                        child, t2_level, state,
					                        level_idx, child_idx);
				} else {

					Ft_initializer_channel::Type_1_level &t1_level =
						channel._t1_levels[level_idx - 1];

					_execute_inner_t1_child(channel, progress,
					                        channel._num_remaining_leaves,
					                        channel._level_to_write,
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
	if (channel._root_node.state != Ft_initializer_channel::Child_state::DONE) {

		Ft_initializer_channel::Type_1_level &t1_level =
			channel._t1_levels[req._ft.max_lvl];

		_execute_inner_t1_child(channel, progress,
		                        channel._num_remaining_leaves,
		                        channel._level_to_write,
		                        channel._root_node.node, t1_level, channel._root_node.state,
		                        req._ft.max_lvl + 1, 0);
		return;
	}

	/*
	 * We will end up here when the root state is 'DONE'.
	 */
	if (channel._num_remaining_leaves == 0)
		_mark_req_successful(channel, progress);
	else
		_mark_req_failed(channel, progress, "initialize FT");
}


void Ft_initializer::_execute_init(Channel &channel,
                                    bool    &progress)
{
	Request &req { *channel._req_ptr };
	switch (channel._state) {
	case Channel::SUBMITTED:

		channel._num_remaining_leaves = req._ft.num_leaves;

		/* clean residual state */
		for (unsigned int i = 0; i < TREE_MAX_LEVEL; i++) {
			Ft_initializer_channel::reset_level(channel._t1_levels[i],
			                                     Ft_initializer_channel::Child_state::DONE);
		}
		channel._level_to_write = 0;

		channel._state = Channel::PENDING;
		channel._root_node.state = Ft_initializer_channel::Child_state::INIT_BLOCK;
		progress = true;

		return;

	case Channel::PENDING:

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:

		_execute(channel, progress);
		return;

	case Channel::BLOCK_IO_COMPLETE:

		_execute(channel, progress);
		return;

	default:
		/*
		 * Omit other states related to ALLOC and IO as those
		 * are handled via Module API.
		 */
		return;
	}
}


void Ft_initializer::_mark_req_failed(Channel    &channel,
                                       bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	channel._req_ptr->_success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Ft_initializer::_mark_req_successful(Channel &channel,
                                           bool    &progress)
{
	Request &req { *channel._req_ptr };

	req._ft.t1_node(channel._root_node.node);
	req._success = true;

	channel._state = Channel::COMPLETE;
	progress = true;
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
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		_execute_init(channel, progress);
	}
}
