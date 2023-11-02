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

/* base includes */
#include <base/log.h>

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/hash.h>
#include <tresor/vbd_initializer.h>

using namespace Tresor;

static constexpr bool DEBUG = false;


Vbd_initializer_request::Vbd_initializer_request(Module_id         src_module_id,
                                                 Module_request_id src_request_id)
:
	Module_request { src_module_id, src_request_id, VBD_INITIALIZER }
{ }


Vbd_initializer_request::Vbd_initializer_request(Module_id         src_module_id,
                        Module_request_id src_request_id,
                   Type    req_type,
                   Tree_level_index  max_level_idx,
                   Tree_node_index  max_child_idx,
                   Number_of_leaves  nr_of_leaves,
                   Pba_allocator &pba_alloc)
:
	Module_request { src_module_id, src_request_id, VBD_INITIALIZER },
	_type { (Type)req_type },
	_max_level_idx { max_level_idx },
	_max_child_idx { max_child_idx },
	_nr_of_leaves  { nr_of_leaves },
	_pba_alloc_ptr { (addr_t)&pba_alloc }
{ }


void Vbd_initializer_request::create(void     *buf_ptr,
                                     size_t    buf_size,
                                     uint64_t  src_module_id,
                                     uint64_t  src_request_id,
                                     size_t    req_type,
                                     uint64_t  max_level_idx,
                                     uint64_t  max_child_idx,
                                     uint64_t  nr_of_leaves,
                                     Pba_allocator &pba_alloc)
{
	Vbd_initializer_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;
	req._max_level_idx = max_level_idx;
	req._max_child_idx = max_child_idx;
	req._nr_of_leaves  = nr_of_leaves;
	req._pba_alloc_ptr   = (addr_t)&pba_alloc;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


char const *Vbd_initializer_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case INIT:    return "init";
	}
	return "?";
}


void Vbd_initializer::_execute_leaf_child(Channel                              &channel,
                                          bool                                 &progress,
                                          uint64_t                             &nr_of_leaves,
                                          Type_1_node                          &child,
                                          Vbd_initializer_channel::Child_state &child_state,
                                          uint64_t                              level_index,
                                          uint64_t                              child_index)
{
	using CS = Vbd_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:
		child_state = CS::INIT_NODE;
		progress = true;
		return;

	case CS::INIT_NODE:
		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " assign pba 0, leaf unused");

			Vbd_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
		} else {

			switch (channel._state) {
			case Channel::IN_PROGRESS:

				Vbd_initializer_channel::reset_node(child);
				if (!channel._request._pba_alloc().alloc(child.pba)) {
					_mark_req_failed(channel, progress, "allocate pba");
					break;
				}
				child_state = CS::DONE;
				--nr_of_leaves;
				progress = true;

				if (DEBUG)
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


void Vbd_initializer::_execute_inner_t1_child(Channel                               &channel,
                                              bool                                  &progress,
                                              uint64_t                               nr_of_leaves,
                                              uint64_t                              &level_to_write,
                                              Type_1_node                           &child,
                                              Vbd_initializer_channel::Type_1_level &child_level,
                                              Vbd_initializer_channel::Child_state  &child_state,
                                              uint64_t                               level_index,
                                              uint64_t                               child_index)

{
	using CS = Vbd_initializer_channel::Child_state;

	switch (child_state) {
	case CS::INIT_BLOCK:

		if (nr_of_leaves == 0) {

			if (DEBUG)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " assign pba 0, inner node unused");

			Vbd_initializer_channel::reset_node(child);
			child_state = CS::DONE;
			progress = true;
			return;
		} else {

			if (DEBUG)
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " reset level: ", level_index - 1);

			Vbd_initializer_channel::reset_level(child_level, CS::INIT_BLOCK);
			child_state = CS::INIT_NODE;
			progress = true;
			return;
		}
		break;

	case CS::INIT_NODE:

		switch (channel._state) {
		case Channel::IN_PROGRESS:
		{
			Vbd_initializer_channel::reset_node(child);
			if (!channel._request._pba_alloc().alloc(child.pba)) {
				_mark_req_failed(channel, progress, "allocate pba");
				break;
			}
			Block blk { };
			child_level.children.encode_to_blk(blk);
			calc_hash(blk, child.hash);

			child_state = CS::WRITE_BLOCK;
			progress = true;

			if (DEBUG)
				log("[vbd_init] node: ", level_index, " ", child_index,
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
				log("[vbd_init] node: ", level_index, " ", child_index,
				    " write pba: ", channel._child_pba, " level: ",
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
		error("vbd initializer: request (", _request, ") failed because generated request failed)");
		_request._success = false;
		_state = COMPLETE;
		//_req_ptr = nullptr;
		return;
	}
	_state = (State)state_uint;
}


void Vbd_initializer_channel::_generate_blk_io_write(bool &progress)
{
	_t1_levels[_level_to_write].children.encode_to_blk(_encoded_blk);
	generate_req<Block_io::Write>(BLOCK_IO_COMPLETE, progress, _child_pba, _encoded_blk, _generated_req_success);
	_state = REQ_GENERATED;
}


void Vbd_initializer::_execute(Channel &channel,
                               bool    &progress)
{
	Request &req { channel._request };

	/*
	 * First handle all child nodes (leaves and inner nodes) that starts after
	 * triggering the root node below.
	 */
	for (uint64_t level_idx = 0; level_idx <= req._max_level_idx; level_idx++) {

		for (uint64_t child_idx = 0; child_idx <= req._max_child_idx; child_idx++) {

			Vbd_initializer_channel::Child_state &state =
				channel._t1_levels[level_idx].children_state[child_idx];

			if (state != Vbd_initializer_channel::Child_state::DONE) {

				Type_1_node &child =
					channel._t1_levels[level_idx].children.nodes[child_idx];

				if (level_idx == 1) {
					_execute_leaf_child(channel, progress, req._nr_of_leaves,
					                    child, state, level_idx, child_idx);
				} else {

					Vbd_initializer_channel::Type_1_level &t1_level =
						channel._t1_levels[level_idx - 1];

					_execute_inner_t1_child(channel, progress,
					                        req._nr_of_leaves,
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
	if (channel._root_node.state != Vbd_initializer_channel::Child_state::DONE) {

		Vbd_initializer_channel::Type_1_level &t1_level =
			channel._t1_levels[req._max_level_idx];

		_execute_inner_t1_child(channel, progress,
		                        req._nr_of_leaves,
		                        channel._level_to_write,
		                        channel._root_node.node, t1_level, channel._root_node.state,
		                        req._max_level_idx + 1, 0);
		return;
	}

	/*
	 * We will end up here when the root state is 'DONE'.
	 */
	if (req._nr_of_leaves == 0)
		_mark_req_successful(channel, progress);
	else
		_mark_req_failed(channel, progress, "initialize VBD");
}


void Vbd_initializer::_execute_init(Channel &channel,
                                    bool    &progress)
{
	switch (channel._state) {
	case Channel::SUBMITTED:

		/* clean residual state */
		for (unsigned int i = 0; i < TREE_MAX_LEVEL; i++) {
			Vbd_initializer_channel::reset_level(channel._t1_levels[i],
			                                     Vbd_initializer_channel::Child_state::DONE);
		}
		channel._level_to_write = 0;

		channel._state = Channel::PENDING;
		channel._root_node.state = Vbd_initializer_channel::Child_state::INIT_BLOCK;
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


void Vbd_initializer::_mark_req_failed(Channel    &channel,
                                       bool       &progress,
                                       char const *str)
{
	error("request failed: failed to ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Vbd_initializer::_mark_req_successful(Channel &channel,
                                           bool    &progress)
{
	Request &req { channel._request };

	memcpy(req._root_node, &channel._root_node.node, sizeof (req._root_node));
	req._success = true;

	channel._state = Channel::COMPLETE;
	progress = true;
}


bool Vbd_initializer::_peek_completed_request(uint8_t *buf_ptr,
                                              size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));
			return true;
		}
	}
	return false;
}


void Vbd_initializer::_drop_completed_request(Module_request &req)
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


Vbd_initializer::Vbd_initializer()
{ register_channels(_channels, NR_OF_CHANNELS, VBD_INITIALIZER); }


bool Vbd_initializer::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}


void Vbd_initializer::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *static_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Vbd_initializer::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		Request &req { channel._request };
		switch (req._type) {
		case Request::INIT:

			_execute_init(channel, progress);

			break;
		default:

			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}
