/*
 * \brief  Module for re-sizing the free tree
 * \author Martin Stein
 * \date   2023-05-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/log.h>

/* tresor tester includes */
#include <tresor/meta_tree.h>
#include <tresor/block_io.h>
#include <tresor/ft_resizing.h>
#include <tresor/hash.h>

using namespace Tresor;

Ft_resizing_request::
Ft_resizing_request(Module_id src_module_id, Module_channel_id src_request_id, Type type, Generation curr_gen,
                    Free_tree_root &ft, Meta_tree_root &mt, Physical_block_address &pba,
                    Number_of_blocks &num_pbas, bool &success)
:
	Module_request { src_module_id, src_request_id, FT_RESIZING }, _type { type }, _curr_gen { curr_gen },
	_ft { ft }, _mt { mt }, _pba { pba }, _num_pbas { num_pbas }, _success { success }
{ }


char const *Ft_resizing_request::type_to_string(Type op)
{
	switch (op) {
	case EXTENSION_STEP: return "extension_step";
	}
	return "?";
}


void Ft_resizing_channel::_generate_write_blk_req(bool &progress)
{
	if (_lvl > 1)
		_t1_blks.items[_lvl].encode_to_blk(_encoded_blk);
	else
		_t2_blk.encode_to_blk(_encoded_blk);

	_generate_req<Block_io::Write>(WRITE_BLK_COMPLETED, progress, _new_pbas.pbas[_lvl], _encoded_blk);
	if (VERBOSE_FT_EXTENSION)
		log("  lvl ", _lvl, " write to pba ", _new_pbas.pbas[_lvl]);
}


void Ft_resizing_channel::_add_new_root_lvl()
{
	Request &req { *_req_ptr };
	ASSERT(req._ft.max_lvl < TREE_MAX_LEVEL);
	req._ft.max_lvl++;
	_t1_blks.items[req._ft.max_lvl] = { };
	_t1_blks.items[req._ft.max_lvl].nodes[0] = req._ft.t1_node();
	_new_pbas.pbas[req._ft.max_lvl] = alloc_pba_from_range(req._pba, req._num_pbas);
	req._ft.t1_node({ _new_pbas.pbas[req._ft.max_lvl], req._curr_gen });
	if (VERBOSE_FT_EXTENSION)
		log("  set root: ", req._ft, "\n  set lvl ", req._ft.max_lvl, " node 0: ",
		    _t1_blks.items[req._ft.max_lvl].nodes[0]);
}


void Ft_resizing_channel::_add_new_branch_at(Tree_level_index dst_lvl, Tree_node_index dst_node_idx)
{
	Request &req { *_req_ptr };
	_num_leaves = 0;
	_lvl = dst_lvl;
	if (dst_lvl > 1) {
		for (Tree_level_index lvl = 1; lvl < dst_lvl; lvl++) {
			if (lvl > 1)
				_t1_blks.items[lvl] = Type_1_node_block { };
			else
				_t2_blk = Type_2_node_block { };

			if (VERBOSE_FT_EXTENSION)
				log("  reset lvl ", lvl);
		}
	}
	for (; _lvl && req._num_pbas; _lvl--) {
		Tree_node_index node_idx = (_lvl == dst_lvl) ? dst_node_idx : 0;
		if (_lvl > 1) {
			_new_pbas.pbas[_lvl - 1] = alloc_pba_from_range(req._pba, req._num_pbas);
			_t1_blks.items[_lvl].nodes[node_idx] = { _new_pbas.pbas[_lvl - 1], req._curr_gen };
			if (VERBOSE_FT_EXTENSION)
				log("  set _lvl d ", _lvl, " node ", node_idx, ": ", _t1_blks.items[_lvl].nodes[node_idx]);

		} else {
			for (; node_idx < req._ft.degree && req._num_pbas; node_idx++) {
				_t2_blk.nodes[node_idx] = { alloc_pba_from_range(req._pba, req._num_pbas) };
				_num_leaves++;
				if (VERBOSE_FT_EXTENSION)
					log("  set _lvl e ", _lvl, " node ", node_idx, ": ", _t2_blk.nodes[node_idx]);
			}
		}
	}
	if (!_lvl)
		_lvl = 1;
}


void Ft_resizing_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = REQ_COMPLETE;
		return;
	}
	_state = (State)state_uint;
}


void Ft_resizing_channel::_mark_req_failed(bool &progress, char const *str)
{
	error(Request::type_to_string(_req_ptr->_type), " request failed, reason: \"", str, "\"");
	_req_ptr->_success = false;
	_state = REQ_COMPLETE;
	progress = true;
}


void Ft_resizing_channel::_extension_step(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_num_leaves = 0;
		_vba = req._ft.num_leaves;
		_old_pbas = { };
		_old_generations = { };
		_new_pbas = { };
		_lvl = req._ft.max_lvl;
		_old_pbas.pbas[_lvl] = req._ft.pba;
		_old_generations.items[_lvl] = req._ft.gen;
		if (_vba <= tree_max_max_vba(req._ft.degree, req._ft.max_lvl)) {

			_generate_req<Block_io::Read>(READ_BLK_COMPLETED, progress, req._ft.pba, _encoded_blk);
			if (VERBOSE_FT_EXTENSION)
				log("  root (", req._ft, "): load to lvl ", _lvl);
		} else {
			_add_new_root_lvl();
			_add_new_branch_at(req._ft.max_lvl, 1);
			_generate_write_blk_req(progress);
			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);
		}
		break;

	case READ_BLK_COMPLETED:

		if (_lvl > 1) {

			_t1_blks.items[_lvl].decode_from_blk(_encoded_blk);
			if (_lvl < req._ft.max_lvl) {
				Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
				if (!check_hash(_encoded_blk, _t1_blks.items[_lvl + 1].nodes[node_idx].hash))
					_mark_req_failed(progress, "hash mismatch");
			} else
				if (!check_hash(_encoded_blk, req._ft.hash))
					_mark_req_failed(progress, "hash mismatch");

			Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl, req._ft.degree);
			Type_1_node &t1_node = _t1_blks.items[_lvl].nodes[node_idx];
			if (t1_node.valid()) {

				_lvl--;
				_old_pbas.pbas [_lvl] = t1_node.pba;
				_old_generations.items[_lvl] = t1_node.gen;
				_generate_req<Block_io::Read>(READ_BLK_COMPLETED, progress, t1_node.pba, _encoded_blk);
				if (VERBOSE_FT_EXTENSION)
					log("  lvl ", _lvl + 1, " node ", node_idx, " (", t1_node, "): load to lvl ", _lvl);
			} else {
				_alloc_lvl = _lvl;
				_add_new_branch_at(_lvl, node_idx);
				if (_old_generations.items[_alloc_lvl] == req._curr_gen) {

					_alloc_pba = _old_pbas.pbas[_alloc_lvl];
					_state = ALLOC_PBA_COMPLETED;
					progress = true;
				} else {
					_alloc_pba = _old_pbas.pbas[_alloc_lvl];
					_generate_req<Meta_tree_request>(
						ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, req._mt, req._curr_gen, _alloc_pba);
				}
			}
		} else {
			_t2_blk.decode_from_blk(_encoded_blk);
			Tree_node_index t1_node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
			if (!check_hash(_encoded_blk, _t1_blks.items[_lvl + 1].nodes[t1_node_idx].hash))
				_mark_req_failed(progress, "hash mismatch");

			Tree_node_index t2_node_idx = t2_node_idx_for_vba(_vba, req._ft.degree);
			if (_t2_blk.nodes[t2_node_idx].valid())
				_mark_req_failed(progress, "t2 node valid");

			_add_new_branch_at(_lvl, t2_node_idx);
			_alloc_lvl = _lvl;
			if (VERBOSE_FT_EXTENSION)
				log("  alloc lvl ", _alloc_lvl);

			_alloc_pba = _old_pbas.pbas[_alloc_lvl];
			_generate_req<Meta_tree_request>(
				ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, req._mt, req._curr_gen, _alloc_pba);
		}
		break;

	case ALLOC_PBA_COMPLETED:

		_new_pbas.pbas[_alloc_lvl] = _alloc_pba;
		if (_alloc_lvl < req._ft.max_lvl) {

			_alloc_lvl++;
			if (_old_generations.items[_alloc_lvl] == req._curr_gen) {

				_alloc_pba = _old_pbas.pbas[_alloc_lvl];
				_state = ALLOC_PBA_COMPLETED;
				progress = true;
			} else {
				_alloc_pba = _old_pbas.pbas[_alloc_lvl];
				_generate_req<Meta_tree_request>(
					ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, req._mt, req._curr_gen, _alloc_pba);
			}
		} else {
			_generate_write_blk_req(progress);
			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);
		}
		break;

	case WRITE_BLK_COMPLETED:

		if (_lvl < req._ft.max_lvl) {
			if (_lvl > 1) {
				Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
				Type_1_node &t1_node { _t1_blks.items[_lvl + 1].nodes[node_idx] };
				t1_node = { _new_pbas.pbas[_lvl], req._curr_gen };
				calc_hash(_encoded_blk, t1_node.hash);
				if (VERBOSE_FT_EXTENSION)
					log("  set lvl ", _lvl + 1, " node ", node_idx, ": ", t1_node);

				_lvl++;
				_generate_write_blk_req(progress);
			} else {
				Tree_node_index node_idx = t1_node_idx_for_vba(_vba, _lvl + 1, req._ft.degree);
				Type_1_node &t1_node = _t1_blks.items[_lvl + 1].nodes[node_idx];
				t1_node = { _new_pbas.pbas[_lvl], req._curr_gen };
				calc_hash(_encoded_blk, t1_node.hash);
				if (VERBOSE_FT_EXTENSION)
					log("  set lvl ", _lvl + 1, " t1_node ", node_idx, ": ", t1_node);

				_lvl++;
				_generate_write_blk_req(progress);
			}
		} else {
			req._ft.t1_node({ _new_pbas.pbas[_lvl], req._curr_gen });
			calc_hash(_encoded_blk, req._ft.hash);
			req._ft.num_leaves += _num_leaves;
			_mark_req_successful(progress);
		}
		break;

	default: break;
	}
}


void Ft_resizing_channel::_mark_req_successful(bool &progress)
{
	_req_ptr->_success = true;
	_state = REQ_COMPLETE;
	progress = true;
}


void Ft_resizing_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::EXTENSION_STEP: _extension_step(progress); break;
	}
}


void Ft_resizing::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


void Ft_resizing_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = REQ_SUBMITTED;
}


Ft_resizing::Ft_resizing()
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}
