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


void Ft_resizing_channel::_set_args_for_write_back_of_inner_lvl(Tree_level_index       const  max_lvl_idx,
                                                        Tree_level_index       const  lvl_idx,
                                                        Physical_block_address const  pba,
                                                        bool                         &progress)
{
	if (lvl_idx == 0) {
		class Program_error_ft_resizing_lvl_idx_zero { };
		throw Program_error_ft_resizing_lvl_idx_zero { };
	}

	if (lvl_idx > max_lvl_idx) {
		class Program_error_ft_resizing_lvl_idx_large { };
		throw Program_error_ft_resizing_lvl_idx_large { };
	}

	if (VERBOSE_FT_EXTENSION)
		log("  lvl ", lvl_idx, " write to pba ", pba);

	_generate_req<Block_io::Write>(
		lvl_idx < max_lvl_idx ? WRITE_INNER_NODE_COMPLETED : WRITE_ROOT_NODE_COMPLETED, progress, pba, _encoded_blk);
}


void Ft_resizing_channel::_add_new_root_lvl_to_ft_using_pba_contingent(Free_tree_root              &ft,
                                                               Generation            const  curr_gen,
                                                               Type_1_node_block_walk &t1_blks,
                                                               Tree_walk_pbas              &new_pbas,
                                                               Physical_block_address      &first_pba,
                                                               Number_of_blocks            &nr_of_pbas)
{
	if (ft.max_lvl >= TREE_MAX_LEVEL) {
		class Program_error_ft_resizing_max_level { };
		throw Program_error_ft_resizing_max_level { };
	}
	ft.max_lvl++;
	t1_blks.items[ft.max_lvl] = { };
	t1_blks.items[ft.max_lvl].nodes[0] = ft.t1_node();
	new_pbas.pbas[ft.max_lvl] = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);
	ft.t1_node({ new_pbas.pbas[ft.max_lvl], curr_gen });
	if (VERBOSE_FT_EXTENSION) {
		log("  set ft root: ", ft);
		log("  set lvl ", ft.max_lvl, " child 0: ", t1_blks.items[ft.max_lvl].nodes[0]);
	}
}


void Ft_resizing_channel::_add_new_branch_to_ft_using_pba_contingent(Tree_level_index      const  mount_point_lvl_idx,
                                                             Tree_node_index       const  mount_point_child_idx,
                                                             Tree_degree           const  ft_degree,
                                                             Generation            const  curr_gen,
                                                             Physical_block_address      &first_pba,
                                                             Number_of_blocks            &nr_of_pbas,
                                                             Type_1_node_block_walk &t1_blks,
                                                             Type_2_node_block           &t2_blk,
                                                             Tree_walk_pbas              &new_pbas,
                                                             Tree_level_index            &stopped_at_lvl_idx,
                                                             Number_of_leaves            &nr_of_leaves)
{
	nr_of_leaves       = 0;
	stopped_at_lvl_idx = mount_point_lvl_idx;

	if (mount_point_lvl_idx > 1) {
		for (unsigned lvl_idx = 1; lvl_idx <= mount_point_lvl_idx - 1; lvl_idx++) {
			if (lvl_idx > 1)
				t1_blks.items[lvl_idx] = Type_1_node_block { };
			else
				t2_blk = Type_2_node_block { };

			if (VERBOSE_FT_EXTENSION)
				log("  reset lvl ", lvl_idx);
		}
	}

	if (nr_of_pbas > 0) {

		for (unsigned lvl_idx = mount_point_lvl_idx; lvl_idx >= 1; lvl_idx --) {
			stopped_at_lvl_idx = lvl_idx;

			if (lvl_idx > 1) {

				if (nr_of_pbas == 0)
					break;

				Tree_node_index const child_idx = (lvl_idx == mount_point_lvl_idx) ? mount_point_child_idx : 0;
				Tree_level_index const child_lvl_idx = lvl_idx - 1;

				new_pbas.pbas[child_lvl_idx] = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);

				t1_blks.items[lvl_idx].nodes[child_idx] = {
					.pba  = new_pbas.pbas[child_lvl_idx],
					.gen  = curr_gen,
					.hash = { }
				};

				if (VERBOSE_FT_EXTENSION)
					log("  set lvl d ", lvl_idx, " child ", child_idx,
					    ": ", t1_blks.items[lvl_idx].nodes[child_idx]);

			} else {
				Tree_node_index const first_child_idx = (lvl_idx == mount_point_lvl_idx) ? mount_point_child_idx : 0;

				for (Tree_node_index child_idx = first_child_idx; child_idx <= ft_degree - 1; child_idx++) {

					if (nr_of_pbas == 0)
						break;

					Physical_block_address child_pba = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);

					t2_blk.nodes[child_idx] = {
						.pba         = child_pba,
						.last_vba    = INVALID_VBA,
						.alloc_gen   = INITIAL_GENERATION,
						.free_gen    = INITIAL_GENERATION,
						.last_key_id = INVALID_KEY_ID,
						.reserved    = false
					};

					if (VERBOSE_FT_EXTENSION)
						log("  set lvl e ", lvl_idx, " child ", child_idx,
						    ": ", t2_blk.nodes[child_idx]);

					nr_of_leaves = nr_of_leaves + 1;
				}
			}
		}
	}
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


void Ft_resizing_channel::_extension_step(bool           &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case REQ_SUBMITTED:

		_nr_of_leaves = 0;
		_vba          = req._ft.num_leaves;

		_old_pbas        = { };
		_old_generations = { };
		_new_pbas        = { };

		_lvl_idx                              = req._ft.max_lvl;
		_old_pbas.pbas[_lvl_idx]         = req._ft.pba;
		_old_generations.items[_lvl_idx] = req._ft.gen;

		if (_vba <= tree_max_max_vba(req._ft.degree, req._ft.max_lvl)) {

			if (VERBOSE_FT_EXTENSION)
				log("  root (", req._ft, "): load to lvl ", _lvl_idx);

			_generate_req<Block_io::Read>(READ_ROOT_NODE_COMPLETED, progress, req._ft.pba, _encoded_blk);

		} else {

			_add_new_root_lvl_to_ft_using_pba_contingent(req._ft,
			                                             req._curr_gen,
			                                             _t1_blks,
			                                             _new_pbas,
			                                             req._pba,
			                                             req._num_pbas);

			_add_new_branch_to_ft_using_pba_contingent(req._ft.max_lvl,
			                                           1,
			                                           req._ft.degree,
			                                           req._curr_gen,
			                                           req._pba,
			                                           req._num_pbas,
			                                           _t1_blks,
			                                           _t2_blk,
			                                           _new_pbas,
			                                           _lvl_idx,
			                                           _nr_of_leaves);

			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);

			_set_args_for_write_back_of_inner_lvl(req._ft.max_lvl,
			                                      _lvl_idx,
			                                      _new_pbas.pbas[_lvl_idx],
			                                      progress);
			if (_lvl_idx > 1)
				_t1_blks.items[_lvl_idx].encode_to_blk(_encoded_blk);
			else
				_t2_blk.encode_to_blk(_encoded_blk);

		}

		break;
	case READ_ROOT_NODE_COMPLETED:
	case READ_INNER_NODE_COMPLETED:
	{
		if (_lvl_idx > 1)
			_t1_blks.items[_lvl_idx].decode_from_blk(_encoded_blk);
		else
			_t2_blk.decode_from_blk(_encoded_blk);

		if (not _generated_req_success) {
			class Primitive_not_successfull_ft_resizing { };
			throw Primitive_not_successfull_ft_resizing { };
		}

		if (_lvl_idx > 1) {

			if (_lvl_idx == req._ft.max_lvl) {

				if (not check_hash(_encoded_blk, req._ft.hash)) {
					class Program_error_ft_resizing_hash_mismatch { };
					throw Program_error_ft_resizing_hash_mismatch { };
				}

			} else {

				Tree_level_index const parent_lvl_idx = _lvl_idx + 1;
				Tree_node_index const child_idx = t1_node_idx_for_vba(_vba, parent_lvl_idx, req._ft.degree);
				Type_1_node const &child = _t1_blks.items[parent_lvl_idx].nodes[child_idx];

				if (not check_hash(_encoded_blk,
											 child.hash)) {
					class Program_error_ft_resizing_hash_mismatch_2 { };
					throw Program_error_ft_resizing_hash_mismatch_2 { };
				}

			}

			Tree_level_index const parent_lvl_idx = _lvl_idx;
			Tree_level_index const child_lvl_idx = _lvl_idx - 1;
			Tree_node_index const child_idx = t1_node_idx_for_vba(_vba, parent_lvl_idx, req._ft.degree);
			Type_1_node const &child = _t1_blks.items[parent_lvl_idx].nodes[child_idx];

			if (child.valid()) {

				_lvl_idx                              = child_lvl_idx;
				_old_pbas.pbas        [child_lvl_idx] = child.pba;
				_old_generations.items[child_lvl_idx] = child.gen;

				_generate_req<Block_io::Read>(READ_INNER_NODE_COMPLETED, progress, child.pba, _encoded_blk);

				if (VERBOSE_FT_EXTENSION)
					log("  lvl ", parent_lvl_idx, " child ", child_idx,
						" (", child, "): load to lvl ", _lvl_idx);

			} else {

				_add_new_branch_to_ft_using_pba_contingent(parent_lvl_idx,
														   child_idx,
														   req._ft.degree,
														   req._curr_gen,
														   req._pba,
														   req._num_pbas,
														   _t1_blks,
														   _t2_blk,
														   _new_pbas,
														   _lvl_idx,
														   _nr_of_leaves);

				_alloc_lvl_idx = parent_lvl_idx;

				if (_old_generations.items[_alloc_lvl_idx] == req._curr_gen) {

					_alloc_pba = _old_pbas.pbas[_alloc_lvl_idx];
					_state = ALLOC_PBA_COMPLETED;
					progress       = true;

				} else {

					_alloc_pba = _old_pbas.pbas[_alloc_lvl_idx];
					generate_req<Meta_tree_request>(
						ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, req._mt,
						req._curr_gen, _alloc_pba, _generated_req_success);
					_state = REQ_GENERATED;
				}
			}
		} else {

			{
				Tree_level_index const parent_lvl_idx = _lvl_idx + 1;
				Tree_node_index const child_idx = t1_node_idx_for_vba(_vba, parent_lvl_idx, req._ft.degree);

				if (not check_hash(_encoded_blk,
											 _t1_blks.items[parent_lvl_idx].nodes[child_idx].hash)) {
					class Program_error_ft_resizing_hash_mismatch_3 { };
					throw Program_error_ft_resizing_hash_mismatch_3 { };
				}
			}

			{
				Tree_level_index const parent_lvl_idx = _lvl_idx;
				Tree_node_index const child_idx = t2_child_idx_for_vba(_vba, req._ft.degree);
				Type_2_node const &child = _t2_blk.nodes[child_idx];

				if (child.valid()) {
					class Program_error_ft_resizing_t2_valid { };
					throw Program_error_ft_resizing_t2_valid { };
				}

				_add_new_branch_to_ft_using_pba_contingent(parent_lvl_idx,
														   child_idx,
														   req._ft.degree,
														   req._curr_gen,
														   req._pba,
														   req._num_pbas,
														   _t1_blks,
														   _t2_blk,
														   _new_pbas,
														   _lvl_idx,
														   _nr_of_leaves);

				_alloc_lvl_idx = parent_lvl_idx;

				if (VERBOSE_FT_EXTENSION)
					log("  alloc lvl ", _alloc_lvl_idx);

				_alloc_pba = _old_pbas.pbas[_alloc_lvl_idx];
				generate_req<Meta_tree_request>(
					ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, req._mt,
					req._curr_gen, _alloc_pba, _generated_req_success);
				_state = REQ_GENERATED;
			}
		}
		break;
	}
	case ALLOC_PBA_COMPLETED:

		_new_pbas.pbas[_alloc_lvl_idx] = _alloc_pba;
		if (_alloc_lvl_idx < req._ft.max_lvl) {

			_alloc_lvl_idx = _alloc_lvl_idx + 1;

			if (_old_generations.items[_alloc_lvl_idx] == req._curr_gen) {

				_alloc_pba = _old_pbas.pbas[_alloc_lvl_idx];
				_state = ALLOC_PBA_COMPLETED;
				progress = true;

			} else {

				_alloc_pba = _old_pbas.pbas[_alloc_lvl_idx];
				generate_req<Meta_tree_request>(
					ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, req._mt,
					req._curr_gen, _alloc_pba, _generated_req_success);
				_state = REQ_GENERATED;
			}

		} else {

			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);

			_set_args_for_write_back_of_inner_lvl(req._ft.max_lvl,
			                                      _lvl_idx,
			                                      _new_pbas.pbas[_lvl_idx],
			                                      progress);
			if (_lvl_idx > 1)
				_t1_blks.items[_lvl_idx].encode_to_blk(_encoded_blk);
			else
				_t2_blk.encode_to_blk(_encoded_blk);

		}
		break;
	case WRITE_INNER_NODE_COMPLETED:

		if (_lvl_idx > 1) {

			Tree_level_index const parent_lvl_idx = _lvl_idx + 1;
			Tree_level_index const child_lvl_idx  = _lvl_idx;
			Tree_node_index const child_idx = t1_node_idx_for_vba(_vba, parent_lvl_idx, req._ft.degree);

			Type_1_node &child {
				_t1_blks.items[parent_lvl_idx].nodes[child_idx] };

			child = {
				.pba     = _new_pbas.pbas[child_lvl_idx],
				.gen     = req._curr_gen,
				.hash    = { },
			};

			calc_hash(_encoded_blk, child.hash);

			if (VERBOSE_FT_EXTENSION)
				log("  set lvl ", parent_lvl_idx, " child ", child_idx,
				    ": ", child);

			_set_args_for_write_back_of_inner_lvl(req._ft.max_lvl,
			                                      parent_lvl_idx,
			                                      _new_pbas.pbas[parent_lvl_idx],
			                                      progress);

			_lvl_idx += 1;

		} else {

			Tree_level_index const parent_lvl_idx = _lvl_idx + 1;
			Tree_level_index const child_lvl_idx = _lvl_idx;
			Tree_node_index const child_idx = t1_node_idx_for_vba(_vba, parent_lvl_idx, req._ft.degree);
			Type_1_node &child = _t1_blks.items[parent_lvl_idx].nodes[child_idx];
			child = {
				.pba = _new_pbas.pbas[child_lvl_idx],
				.gen = req._curr_gen,
			};

			calc_hash(_encoded_blk, child.hash);

			if (VERBOSE_FT_EXTENSION)
				log("  set lvl ", parent_lvl_idx, " child ", child_idx,
				    ": ", child);

			_set_args_for_write_back_of_inner_lvl(req._ft.max_lvl,
			                                      parent_lvl_idx,
			                                      _new_pbas.pbas[parent_lvl_idx],
			                                      progress);

			_lvl_idx += 1; // = 2
		}
		if (_lvl_idx > 1)
			_t1_blks.items[_lvl_idx].encode_to_blk(_encoded_blk);
		else
			_t2_blk.encode_to_blk(_encoded_blk);
		break;

	case WRITE_ROOT_NODE_COMPLETED: {

		req._ft.t1_node({ _new_pbas.pbas[_lvl_idx], req._curr_gen });
		calc_hash(_encoded_blk, req._ft.hash);
		req._ft.num_leaves += _nr_of_leaves;
		req._success = true;
		_state = REQ_COMPLETE;
		progress = true;
		break;
	}
	default: break;
	}
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
