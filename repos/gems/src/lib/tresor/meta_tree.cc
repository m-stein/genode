/*
 * \brief  Module for doing free tree COW allocations on the meta tree
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* tresor includes */
#include <tresor/meta_tree.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;
enum{VERBOSE=1};
enum{VERBOSE_X=0};

/***************
 ** Utilities **
 ***************/

static bool check_level_0_usable(Generation   gen,
                                 Type_2_node &node)
{
	return node.alloc_gen != gen;
}


Meta_tree_request::Meta_tree_request(Module_id src_module_id,
                                     Module_channel_id src_channel_id,
                                     Type type,
                                     Meta_tree_root &mt,
                                     Generation curr_gen,
                                     Physical_block_address &pba,
                                     bool &success)
:
	Module_request { src_module_id, src_channel_id, META_TREE }, _type { type }, _mt { mt },
	_curr_gen { curr_gen }, _pba { pba }, _success { success }
{ }


char const *Meta_tree_request::type_to_string(Type type)
{
	switch (type) {
	case ALLOC_PBA: return "update";
	}
	return "?";
}


bool Meta_tree::_peek_generated_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { _channels[id] };
		Local_cache_request const &local_req { channel._cache_request };
		if (local_req.state == Local_cache_request::PENDING) {

			Block_io_request::Type blk_io_req_type;
			switch(local_req.op) {
			case Local_cache_request::READ: blk_io_req_type = Block_io_request::READ;
break;
			case Local_cache_request::WRITE: blk_io_req_type = Block_io_request::WRITE;
break;
			default: ASSERT_NEVER_REACHED;
			}
			ASSERT(sizeof(Block_io_request) <= buf_size);
			construct_at<Block_io_request>(
				buf_ptr, META_TREE, id, blk_io_req_type,
				0, 0, 0, local_req.pba, 0, 1,
				channel._cache_request.block_data, channel._dummy_hash, channel._generated_req_success);

			return true;
		}
	}
	return false;
}


void Meta_tree::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Local_cache_request &local_req { _channels[id]._cache_request };
	if (local_req.state != Local_cache_request::PENDING) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	local_req.state = Local_cache_request::IN_PROGRESS;
}


void Meta_tree::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Local_cache_request &local_req { _channels[id]._cache_request };
	if (local_req.state != Local_cache_request::IN_PROGRESS) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	if (mod_req.dst_module_id() != BLOCK_IO) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	Channel &channel { _channels[id] };
	if (!channel._generated_req_success) {

		channel._request->_success = false;
		channel._state = Channel::COMPLETE;
		return;

	}
	Type_1_info &t1_info { channel._level_n_nodes[local_req.level] };
	Type_2_info &t2_info { channel._level_1_node };

	switch (local_req.op) {
	case Local_cache_request::SYNC:

		class Exception_3 { };
		throw Exception_3 { };

	case Local_cache_request::READ:

		if (local_req.level > T2_NODE_LVL) {

			if (!check_hash(channel._cache_request.block_data, t1_info.node.hash)) {

				channel._state = Channel::TREE_HASH_MISMATCH;

			} else {

				t1_info.entries.decode_from_blk(channel._cache_request.block_data);
				t1_info.index = 0;
				t1_info.state = Type_1_info::READ_COMPLETE;
			}
		} else if (local_req.level == T2_NODE_LVL) {

			if (!check_hash(channel._cache_request.block_data, t2_info.node.hash)) {

				channel._state = Channel::TREE_HASH_MISMATCH;

			} else {

				t2_info.entries.decode_from_blk(channel._cache_request.block_data);
				t2_info.index = 0;
				t2_info.state = Type_2_info::READ_COMPLETE;
			}
		} else {
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;

	case Local_cache_request::WRITE:

		if (local_req.level > T2_NODE_LVL) {

			t1_info.state = Type_1_info::WRITE_COMPLETE;

		} else if (local_req.level == T2_NODE_LVL) {

			t2_info.state = Type_2_info::WRITE_COMPLETE;

		} else {

			class Exception_5 { };
			throw Exception_5 { };
		}
		break;
	}
	local_req = Local_cache_request {
		Local_cache_request::INVALID, Local_cache_request::READ,
		false, 0, 0, nullptr };
}


void Meta_tree::_mark_req_failed(Channel    &chan,
                                 bool       &progress,
                                 char const *str)
{
	error(chan._request->type_to_string(chan._request->_type), " request failed, reason: \"", str, "\"");
	chan._request->_success = false;
	chan._state = Channel::COMPLETE;
	progress = true;
}


void Meta_tree::_mark_req_successful(Channel &channel,
                                     bool    &progress)
{
	channel._request->_success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Meta_tree::_update_parent(Type_1_node &node,
                               Block const &blk,
                               uint64_t     gen,
                               uint64_t     pba)
{
	calc_hash(blk, node.hash);
	node.gen = gen;
	node.pba = pba;
}


void Meta_tree::_exchange_nv_inner_nodes(Channel     &channel,
                                         Type_2_node &t2_entry,
                                         bool        &exchanged, uint64_t idx)
{
	Request &req { *channel._request };
	uint64_t pba;
	exchanged = false;

	// loop non-volatile inner nodes
	for (uint64_t lvl { MT_LOWEST_T1_LVL }; lvl <= TREE_MAX_LEVEL; lvl++) {

		Type_1_info &t1_info { channel._level_n_nodes[lvl] };
		if (t1_info.node.valid() && !t1_info.volatil) {

			pba = t1_info.node.pba;
			t1_info.node.pba   = t2_entry.pba;
			t1_info.node.gen   = req._curr_gen;
			t1_info.volatil    = true;
			t2_entry.pba       = pba;
			t2_entry.alloc_gen = req._curr_gen;
			t2_entry.free_gen  = req._curr_gen;
			t2_entry.reserved  = false;

			exchanged = true;

if (VERBOSE_X) log("    ", idx, ": lvl ", lvl);
			break;
		}
	}
}


void Meta_tree::_exchange_nv_level_1_node(Channel     &channel,
                                          Type_2_node &t2_entry,
                                          bool        &exchanged)
{
	Request &req { *channel._request };
	uint64_t pba { channel._level_1_node.node.pba };
	exchanged = false;

	if (!channel._level_1_node.volatil) {

		channel._level_1_node.node.pba = t2_entry.pba;
		channel._level_1_node.volatil  = true;

		t2_entry.pba       = pba;
		t2_entry.alloc_gen = req._curr_gen;
		t2_entry.free_gen  = req._curr_gen;
		t2_entry.reserved  = false;

		exchanged = true;
	}
}


void Meta_tree::_exchange_request_pba(Channel     &channel,
                                      Type_2_node &t2_entry)
{
	Request &req { *channel._request };
	req._success = true;
	Physical_block_address old_pba = req._pba;
	req._pba = t2_entry.pba;
	channel._finished = true;

	t2_entry.pba       = old_pba;
	t2_entry.alloc_gen = req._curr_gen;
	t2_entry.free_gen  = req._curr_gen;
	t2_entry.reserved  = false;
}


void Meta_tree::_handle_level_0_nodes(Channel &channel,
                                      bool    &handled)
{
	ASSERT(!channel._started_exchange_request_pba);
	ASSERT(!channel._started_exchange_level_n);

	Request &req { *channel._request };
	Type_2_node tmp_t2_entry;
	handled = false;

	for(unsigned i = 0; i <= req._mt.degree - 1; i++) {


		tmp_t2_entry = channel._level_1_node.entries.nodes[i];

		if (tmp_t2_entry.valid() &&
			check_level_0_usable(req._curr_gen, tmp_t2_entry))
		{
			bool exchanged_level_1 { false };
			bool exchanged_level_n { false };
			bool exchanged_request_pba { false };

			if (!channel._started_exchange_request_pba) {
Type_2_node ot2 = tmp_t2_entry;
				channel._started_exchange_request_pba = true;
				_exchange_request_pba(channel, tmp_t2_entry);
				exchanged_request_pba = true;
if (VERBOSE) log("  0.", i, " a ", ot2, " -> ", tmp_t2_entry);
			}
			if (!exchanged_request_pba)
				_exchange_nv_level_1_node(
					channel, tmp_t2_entry, exchanged_level_1);

if (exchanged_level_1)
if (VERBOSE_X) log("    ", i, ": lvl ", 1);

			if (!exchanged_request_pba && !exchanged_level_1) {
				channel._started_exchange_level_n = true;
				_exchange_nv_inner_nodes(
					channel, tmp_t2_entry, exchanged_level_n, i);
			}

			channel._level_1_node.entries.nodes[i] = tmp_t2_entry;
			handled = true;

			if (channel._started_exchange_level_n && !exchanged_level_n)
				return;
		} else {

if (VERBOSE_X) log("    ", i, ": ", tmp_t2_entry.valid(), " ", tmp_t2_entry.alloc_gen, " ", req._curr_gen);
		}
	}
}


void Meta_tree::_handle_level_1_node(Channel &channel,
                                     bool    &handled)
{
	Type_1_info &t1_info { channel._level_n_nodes[MT_LOWEST_T1_LVL] };
	Type_2_info &t2_info { channel._level_1_node };
	Request &req { *channel._request };

	switch (t2_info.state) {
	case Type_2_info::INVALID:

		handled = false;
		break;

	case Type_2_info::READ:

if (VERBOSE) log("  1.", t1_info.index, " r ", t1_info.entries.nodes[t1_info.index].pba, " ", t1_info.entries.nodes[t1_info.index].gen);
		channel._cache_request = Local_cache_request {
			Local_cache_request::PENDING, Local_cache_request::READ, false,
			t2_info.node.pba, 1, nullptr };

		handled = true;
		break;

	case Type_2_info::READ_COMPLETE:

		_handle_level_0_nodes(channel, handled);
		if (handled) {
			t2_info.state = Type_2_info::WRITE;
		} else {
			t2_info.state = Type_2_info::COMPLETE;
			handled = true;
		}
		break;

	case Type_2_info::WRITE:
	{
		Block block_data { };
		t2_info.entries.encode_to_blk(block_data);

		_update_parent(
			t1_info.entries.nodes[t1_info.index], block_data,
			req._curr_gen, t2_info.node.pba);

if (VERBOSE) log("  1.", t1_info.index, " w ", t1_info.entries.nodes[t1_info.index].pba, " ", t1_info.entries.nodes[t1_info.index].gen);
		channel._cache_request = Local_cache_request {
			Local_cache_request::PENDING, Local_cache_request::WRITE, false,
			t2_info.node.pba, 1, &block_data };

		t1_info.dirty = true;
		handled = true;
		break;
	}
	case Type_2_info::WRITE_COMPLETE:

		t1_info.index++;
		t2_info.state = Type_2_info::INVALID;
		handled = true;
		break;

	case Type_2_info::COMPLETE:

		t1_info.index++;
		t2_info.state = Type_2_info::INVALID;
		handled = true;
		break;
	}
}


void Meta_tree::_execute_update(Channel &channel,
                                bool    &progress)
{
	bool handled_level_1_node;
	bool handled_level_n_nodes;
	_handle_level_1_node(channel, handled_level_1_node);
	if (handled_level_1_node) {
		progress = true;
		return;
	}
	_handle_level_n_nodes(channel, handled_level_n_nodes);
	progress = progress || handled_level_n_nodes;
}


void Meta_tree::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._cache_request.state != Local_cache_request::INVALID)
			continue;

		switch(channel._state) {
		case Channel::INVALID:
			break;
		case Channel::UPDATE:
			_execute_update(channel, progress);
			break;
		case Channel::COMPLETE:
			break;
		case Channel::TREE_HASH_MISMATCH:
			_mark_req_failed(channel, progress, "node hash mismatch");
			break;
		}
	}
}


Meta_tree::Meta_tree() { }


bool Meta_tree::_peek_completed_request(uint8_t *buf_ptr,
                                        size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			if (sizeof(channel._request) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &channel._request, sizeof(channel._request));

			Request &r { *channel._request };
			construct_at<Request>(buf_ptr, r.src_module_id(), r.src_chan_id(), r._type,
				r._mt, r._curr_gen, r._pba, r._success);
			(*(Request*)buf_ptr).dst_request_id(r.dst_chan_id());
			return true;
		}
	}
	return false;
}


void Meta_tree::_drop_completed_request(Module_request &req)
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
	_channels[id]._state = Channel::INVALID;
}


bool Meta_tree::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INVALID)
			return true;
	}
	return false;
}


bool Meta_tree::_node_volatile(Type_1_node const &node,
                               uint64_t           gen)
{
   return node.gen == 0 || node.gen == gen;
}


void Meta_tree::submit_request(Module_request &mod_req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._state == Channel::INVALID) {

			mod_req.dst_request_id(id);

			Request &r { *static_cast<Request *>(&mod_req) };
			chan._request.construct(
				r.src_module_id(), r.src_chan_id(), r._type, r._mt, r._curr_gen, r._pba, r._success);
			chan._request->dst_request_id(id);

			chan._finished = false;
			chan._state = Channel::UPDATE;
			for (Type_1_info &t1_info : chan._level_n_nodes) {
				t1_info = Type_1_info { };
			}
			chan._level_1_node = Type_2_info { };

			Request &req { *chan._request };
			Type_1_node root_node { };
			root_node.pba = req._mt.pba;
			root_node.gen = req._mt.gen;
			root_node.hash = req._mt.hash;

			chan._level_n_nodes[req._mt.max_lvl].index = 0;
			chan._level_n_nodes[req._mt.max_lvl].node = root_node;
			chan._level_n_nodes[req._mt.max_lvl].state = Type_1_info::READ;
			chan._level_n_nodes[req._mt.max_lvl].volatil =
				_node_volatile(root_node, req._curr_gen);

			chan._started_exchange_request_pba = false;
			chan._started_exchange_level_n = false;
if (VERBOSE) log("submit mt ", req._mt.pba, " ", req._mt.gen, " pba ", r._pba, " gen ", req._curr_gen);

			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


void Meta_tree::_handle_level_n_nodes(Channel &channel,
                                      bool    &handled)
{
	Request &req { *channel._request };
	handled = false;

	for (uint64_t lvl { MT_LOWEST_T1_LVL }; lvl <= TREE_MAX_LEVEL; lvl++) {

		Type_1_info &t1_info { channel._level_n_nodes[lvl] };

		switch (t1_info.state) {
		case Type_1_info::INVALID:

			break;

		case Type_1_info::READ:

if (VERBOSE) log("  ", lvl, ".", t1_info.index, " r ", t1_info.node.pba, " ", t1_info.node.gen);
			channel._cache_request = Local_cache_request {
				Local_cache_request::PENDING, Local_cache_request::READ, false,
				t1_info.node.pba, lvl, nullptr };

			handled = true;
			return;

		case Type_1_info::READ_COMPLETE:

			if (t1_info.index < req._mt.degree &&
				t1_info.entries.nodes[t1_info.index].valid() &&
				!channel._finished) {

				if (lvl != MT_LOWEST_T1_LVL) {
					channel._level_n_nodes[lvl - 1] = {
						Type_1_info::READ, t1_info.entries.nodes[t1_info.index],
						{ }, 0, false,
						_node_volatile(t1_info.node, req._curr_gen) };

				} else {
					channel._level_1_node = {
						Type_2_info::READ, t1_info.entries.nodes[t1_info.index],
						{ }, 0,
						_node_volatile(t1_info.node, req._curr_gen) };
				}

			} else {

				if (t1_info.dirty)
					t1_info.state = Type_1_info::WRITE;
				else
					t1_info.state = Type_1_info::COMPLETE;
			}
			handled = true;
			return;

		case Type_1_info::WRITE:
		{
			Block block_data;
			t1_info.entries.encode_to_blk(block_data);

			if (lvl == req._mt.max_lvl) {

				Type_1_node root_node { };
				root_node.pba  = req._mt.pba;
				root_node.gen  = req._mt.gen;
				root_node.hash = req._mt.hash;

				_update_parent(
					root_node, block_data, req._curr_gen,
					t1_info.node.pba);

				req._mt.pba = root_node.pba;
				req._mt.gen = root_node.gen;
				req._mt.hash = root_node.hash;

				channel._root_dirty = true;
if (VERBOSE) log("  ", lvl, ".", 0 , " w ", req._mt.pba, " ", req._mt.gen);

			} else {

				Type_1_info &parent { channel._level_n_nodes[lvl + 1] };
				_update_parent(
					parent.entries.nodes[parent.index], block_data,
					req._curr_gen, t1_info.node.pba);

				parent.dirty = true;
if (VERBOSE) log("  ", lvl, ".", parent.index, " w ", parent.entries.nodes[parent.index].pba, " ", parent.entries.nodes[parent.index].gen);
			}
			channel._cache_request = Local_cache_request {
				Local_cache_request::PENDING, Local_cache_request::WRITE,
				false, t1_info.node.pba, lvl, &block_data };

			handled = true;
			return;
		}
		case Type_1_info::WRITE_COMPLETE:

			if (lvl == req._mt.max_lvl)
				channel._state = Channel::COMPLETE;
			else
				channel._level_n_nodes[lvl + 1].index++;

			channel._cache_request = Local_cache_request {
				Local_cache_request::INVALID, Local_cache_request::READ,
				false, 0, 0, nullptr };

			t1_info.state = Type_1_info::INVALID;
			handled = true;
			return;

		case Type_1_info::COMPLETE:

			if (lvl == req._mt.max_lvl)
				channel._state = Channel::COMPLETE;
			else
				channel._level_n_nodes[lvl + 1].index++;

			t1_info.state = Type_1_info::INVALID;
			handled = true;
			return;
		}
	}
}
