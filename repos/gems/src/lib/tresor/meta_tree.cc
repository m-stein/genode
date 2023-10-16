/*
 * \brief  Module for doing VBD COW allocations on the meta tree
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
enum{VERBOSE= 0};

char const *Meta_tree_request::type_to_string(Type type)
{
	switch (type) {
	case ALLOC_PBA: return "alloc pba";
	}
	ASSERT_NEVER_REACHED;
}


Meta_tree_request::Meta_tree_request(Module_id src_module_id, Module_channel_id src_channel_id,
                                     Type type, Meta_tree_root &mt, Generation curr_gen,
                                     Physical_block_address &pba, bool &success)
:
	Module_request { src_module_id, src_channel_id, META_TREE }, _type { type }, _mt { mt },
	_curr_gen { curr_gen }, _pba { pba }, _success { success }
{ }


void Meta_tree::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


bool Meta_tree_channel::_can_alloc_pba_of(Type_2_node &node)
{
	return node.valid() && node.alloc_gen != _req_ptr->_curr_gen;
}


void Meta_tree_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("meta tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		return;
	}
	_state = (State)state_uint;
}


void Meta_tree_channel::_alloc_pba_of(Type_2_node &t2_node, Physical_block_address &pba)
{
	Request &req { *_req_ptr };
	Physical_block_address old_pba { pba };
	pba = t2_node.pba;
	t2_node.pba = old_pba;
	t2_node.alloc_gen = req._curr_gen;
	t2_node.free_gen = req._curr_gen;
	t2_node.reserved = false;
}


void Meta_tree_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"", str, "\"");
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Meta_tree_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = COMPLETE;
	progress = true;
}


void Meta_tree_channel::_start_tree_traversal(bool &progress)
{
	Request &req { *_req_ptr };
	_pba_allocated = false;
	_lvl = req._mt.max_lvl;
	_node_idx[_lvl] = 0;
	_t1_blks[_lvl].nodes[_node_idx[_lvl]] = req._mt.t1_node();
if (VERBOSE) log("  ", _lvl,".", _node_idx[_lvl],  " r ", req._mt.pba, " ", req._mt.gen);
	_generate_req<Block_io::Read>(SEEK_DOWN, progress, req._mt.pba, _blk);
}


void Meta_tree_channel::_traverse_curr_node(bool &progress)
{
	Request &req { *_req_ptr };
	if (_lvl) {
		Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
		if (t1_node.pba)
{
if (VERBOSE) log("  ", _lvl,".", _node_idx[_lvl],  " r ", t1_node.pba, " ", t1_node.gen);
			_generate_req<Block_io::Read>(SEEK_DOWN, progress, t1_node.pba, _blk);
}
		else {
			_state = SEEK_LEFT_OR_UP;
			progress = true;
		}
	} else {
		Type_2_node &t2_node { _t2_blk.nodes[_node_idx[_lvl]] };
		ASSERT(!_pba_allocated);
		if (_can_alloc_pba_of(t2_node)) {
Type_2_node ot2 = t2_node;
			_alloc_pba_of(t2_node, _req_ptr->_pba);
if (VERBOSE) log("  ",_lvl,".", _node_idx[_lvl], " a ", ot2, " -> ", t2_node);
			_pba_allocated = true;
		}
		for (Tree_level_index lvl { 1 }; lvl <= req._mt.max_lvl; lvl++) {
			Type_1_node &t1_node { _t1_blks[lvl].nodes[_node_idx[lvl]] };
			if (!t1_node.is_volatile(req._curr_gen)) {
				bool pba_allocated { false };
				for (Type_2_node &t2_node : _t2_blk.nodes) {
					if (_can_alloc_pba_of(t2_node)) {
						_alloc_pba_of(t2_node, t1_node.pba);
						pba_allocated = true;
						break;
					}
				}
				ASSERT(pba_allocated);
			}
		}
		_state = SEEK_LEFT_OR_UP;
		progress = true;
	}
}


void Meta_tree_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_start_tree_traversal(progress);
		break;

	case SEEK_DOWN:
	{
		if (!check_hash(_blk, _t1_blks[_lvl].nodes[_node_idx[_lvl]].hash)) {
			_mark_req_failed(progress, "hash mismatch");
			break;
		}
		_lvl--;
		_node_idx[_lvl] = 0;
		if (_lvl)
			_t1_blks[_lvl].decode_from_blk(_blk);
		else
{
			_t2_blk.decode_from_blk(_blk);
if (VERBOSE) log(_t2_blk);
}
		_traverse_curr_node(progress);
		break;
	}
	case SEEK_LEFT_OR_UP:

		if (_lvl < req._mt.max_lvl) {
			if (_node_idx[_lvl] < req._mt.degree - 1 && !_pba_allocated) {
				_node_idx[_lvl]++;
				_traverse_curr_node(progress);
			} else {
				_lvl++;
				_state = _pba_allocated ? WRITE_BLK : SEEK_LEFT_OR_UP;
				progress = true;
			}
		} else {
			if (_pba_allocated) {
				req._mt.t1_node(_t1_blks[_lvl].nodes[_node_idx[_lvl]]);
				_mark_req_successful(progress);
			} else
				_mark_req_failed(progress, "not enough free pbas");
		}
		break;

	case WRITE_BLK:
	{
		if (_lvl > 1)
			_t1_blks[_lvl - 1].encode_to_blk(_blk);
		else
			_t2_blk.encode_to_blk(_blk);
		Type_1_node &t1_node { _t1_blks[_lvl].nodes[_node_idx[_lvl]] };
		t1_node.gen = req._curr_gen;
		calc_hash(_blk, t1_node.hash);
if (VERBOSE) log("  ", _lvl,".", _node_idx[_lvl], " w ", t1_node.pba, " ", t1_node.gen);
		_generate_req<Block_io::Write>(SEEK_LEFT_OR_UP, progress, t1_node.pba, _blk);
		break;
	}
	default: break;
	}
}


void Meta_tree_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
if (VERBOSE) log("submit mt ", _req_ptr->_mt.pba, " ", _req_ptr->_mt.gen, " pba ", _req_ptr->_pba, " gen ", _req_ptr->_curr_gen);
}


Meta_tree::Meta_tree()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}
