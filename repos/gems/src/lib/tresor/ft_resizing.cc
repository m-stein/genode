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


/*************************
 ** Ft_resizing_request **
 *************************/

Ft_resizing_request::
Ft_resizing_request(Module_id src_module_id,
                    Module_channel_id src_request_id,
                    Type type,
                    Generation curr_gen,
                    Type_1_node &ft_root,
                    Tree_level_index &ft_max_lvl,
                    Number_of_leaves &ft_nr_of_leaves,
                    Tree_degree ft_degree,
                    Physical_block_address &mt_root_pba,
                    Generation &mt_root_gen,
                    Hash &mt_root_hash,
                    Tree_level_index mt_max_level,
                    Tree_degree mt_degree,
                    Number_of_leaves mt_leaves,
                    Physical_block_address &pba,
                    Number_of_blocks &nr_of_pbas,
                    bool &success)
:
	Module_request { src_module_id, src_request_id, FT_RESIZING },
	_type { type },
	_curr_gen { curr_gen },
	_ft_root_ptr { (addr_t)&ft_root },
	_ft_max_lvl_ptr { (addr_t)&ft_max_lvl },
	_ft_nr_of_leaves_ptr { (addr_t)&ft_nr_of_leaves },
	_ft_degree { ft_degree },
	_mt_root_pba_ptr { (addr_t)&mt_root_pba },
	_mt_root_gen_ptr { (addr_t)&mt_root_gen },
	_mt_root_hash_ptr { (addr_t)&mt_root_hash },
	_mt_max_level { mt_max_level },
	_mt_degree { mt_degree },
	_mt_leaves { mt_leaves },
	_pba_ptr { (addr_t)&pba },
	_nr_of_pbas_ptr { (addr_t)&nr_of_pbas },
	_success_ptr { (addr_t)&success }
{ }

char const *Ft_resizing_request::type_to_string(Type op)
{
	switch (op) {
	case INVALID: return "invalid";
	case FT_EXTENSION_STEP: return "ft_ext_step";
	}
	return "?";
}


/*************************
 ** Ft_resizing_request **
 *************************/

void Ft_resizing::_execute_ft_ext_step_read_inner_node_completed(Channel        &channel,
                                                                 unsigned const  job_idx,
                                                                 bool           &progress)
{
	Request &req { channel._request };
	if (not channel._generated_prim.succ) {
		class Primitive_not_successfull_ft_resizing { };
		throw Primitive_not_successfull_ft_resizing { };
	}

	if (channel._lvl_idx > 1) {

		if (channel._lvl_idx == req._ft_max_lvl()) {

			if (not check_hash(channel._encoded_blk,
                                         req._ft_root().hash)) {
				class Program_error_ft_resizing_hash_mismatch { };
				throw Program_error_ft_resizing_hash_mismatch { };
			}

		} else {

			Tree_level_index const parent_lvl_idx = channel._lvl_idx + 1;
			Tree_node_index const child_idx = t1_node_idx_for_vba(channel._vba, parent_lvl_idx, req._ft_degree);
			Type_1_node const &child = channel._t1_blks.items[parent_lvl_idx].nodes[child_idx];

			if (not check_hash(channel._encoded_blk,
			                             child.hash)) {
				class Program_error_ft_resizing_hash_mismatch_2 { };
				throw Program_error_ft_resizing_hash_mismatch_2 { };
			}

		}

		Tree_level_index const parent_lvl_idx = channel._lvl_idx;
		Tree_level_index const child_lvl_idx = channel._lvl_idx - 1;
		Tree_node_index const child_idx = t1_node_idx_for_vba(channel._vba, parent_lvl_idx, req._ft_degree);
		Type_1_node const &child = channel._t1_blks.items[parent_lvl_idx].nodes[child_idx];

		if (child.valid()) {

			channel._lvl_idx                              = child_lvl_idx;
			channel._old_pbas.pbas        [child_lvl_idx] = child.pba;
			channel._old_generations.items[child_lvl_idx] = child.gen;

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_FT_RSZG_CACHE,
				.blk_nr = child.pba,
				.idx    = job_idx
			};

			channel._state = Channel::State::READ_INNER_NODE_PENDING;
			progress       = true;

			if (VERBOSE_FT_EXTENSION)
				log("  lvl ", parent_lvl_idx, " child ", child_idx,
				    " (", child, "): load to lvl ", channel._lvl_idx);

		} else {

			_add_new_branch_to_ft_using_pba_contingent(parent_lvl_idx,
			                                           child_idx,
			                                           req._ft_degree,
			                                           req._curr_gen,
			                                           req._pba(),
			                                           req._nr_of_pbas(),
			                                           channel._t1_blks,
			                                           channel._t2_blk,
			                                           channel._new_pbas,
			                                           channel._lvl_idx,
			                                           channel._nr_of_leaves);

			channel._alloc_lvl_idx = parent_lvl_idx;

			if (channel._old_generations.items[channel._alloc_lvl_idx] == req._curr_gen) {

				channel._alloc_pba = channel._old_pbas.pbas[channel._alloc_lvl_idx];
				channel._state = Channel::State::ALLOC_PBA_COMPLETED;
				progress       = true;

			} else {

				channel._generated_prim = {
					.op     = Channel::Generated_prim::Type::READ,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_FT_RSZG_MT_ALLOC,
					.blk_nr = 0,
					.idx    = job_idx
				};
				channel._alloc_pba = channel._old_pbas.pbas[channel._alloc_lvl_idx];
				channel.generate_req<Meta_tree_request>(
					Channel::ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, *channel._mt,
					req._curr_gen, channel._alloc_pba, channel._generated_prim.succ);
				channel._state = Channel::REQ_GENERATED;
			}
		}
	} else {

		{
			Tree_level_index const parent_lvl_idx = channel._lvl_idx + 1;
			Tree_node_index const child_idx = t1_node_idx_for_vba(channel._vba, parent_lvl_idx, req._ft_degree);

			if (not check_hash(channel._encoded_blk,
			                             channel._t1_blks.items[parent_lvl_idx].nodes[child_idx].hash)) {
				class Program_error_ft_resizing_hash_mismatch_3 { };
				throw Program_error_ft_resizing_hash_mismatch_3 { };
			}
		}

		{
			Tree_level_index const parent_lvl_idx = channel._lvl_idx;
			Tree_node_index const child_idx = t2_child_idx_for_vba(channel._vba, req._ft_degree);
			Type_2_node const &child = channel._t2_blk.nodes[child_idx];

			if (child.valid()) {
				class Program_error_ft_resizing_t2_valid { };
				throw Program_error_ft_resizing_t2_valid { };
			}

			_add_new_branch_to_ft_using_pba_contingent(parent_lvl_idx,
			                                           child_idx,
			                                           req._ft_degree,
			                                           req._curr_gen,
			                                           req._pba(),
			                                           req._nr_of_pbas(),
			                                           channel._t1_blks,
			                                           channel._t2_blk,
			                                           channel._new_pbas,
			                                           channel._lvl_idx,
			                                           channel._nr_of_leaves);

			channel._alloc_lvl_idx = parent_lvl_idx;

			if (VERBOSE_FT_EXTENSION)
				log("  alloc lvl ", channel._alloc_lvl_idx);

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_FT_RSZG_MT_ALLOC,
				.blk_nr = 0,
				.idx    = job_idx
			};
			channel._alloc_pba = channel._old_pbas.pbas[channel._alloc_lvl_idx];
			channel.generate_req<Meta_tree_request>(
				Channel::ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, *channel._mt,
				req._curr_gen, channel._alloc_pba, channel._generated_prim.succ);
			channel._state = Channel::REQ_GENERATED;
		}
	}
}


void Ft_resizing::_set_args_for_write_back_of_inner_lvl(Channel &chan, Tree_level_index       const  max_lvl_idx,
                                                        Tree_level_index       const  lvl_idx,
                                                        Physical_block_address const  pba,
                                                        unsigned               const  prim_idx,
                                                        Channel::State               &,
                                                        bool                         &progress,
                                                        Channel::Generated_prim      &prim)
{
	if (lvl_idx == 0) {
		class Program_error_ft_resizing_lvl_idx_zero { };
		throw Program_error_ft_resizing_lvl_idx_zero { };
	}

	if (lvl_idx > max_lvl_idx) {
		class Program_error_ft_resizing_lvl_idx_large { };
		throw Program_error_ft_resizing_lvl_idx_large { };
	}

	prim = {
		.op     = Channel::Generated_prim::Type::WRITE,
		.succ   = false,
		.tg     = Channel::Tag_type::TAG_FT_RSZG_CACHE,
		.blk_nr = pba,
		.idx    = prim_idx
	};

	if (VERBOSE_FT_EXTENSION)
		log("  lvl ", lvl_idx, " write to pba ", pba);

	chan._generate_req<Block_io::Write>(
		lvl_idx < max_lvl_idx ? Channel::WRITE_INNER_NODE_COMPLETED : Channel::WRITE_ROOT_NODE_COMPLETED, progress, pba, chan._encoded_blk);
}


void Ft_resizing::_add_new_root_lvl_to_ft_using_pba_contingent(Type_1_node                 &ft_root,
                                                               Tree_level_index            &ft_max_lvl_idx,
                                                               Number_of_leaves const       ft_nr_of_leaves,
                                                               Generation            const  curr_gen,
                                                               Channel::Type_1_node_blocks &t1_blks,
                                                               Tree_walk_pbas              &new_pbas,
                                                               Physical_block_address      &first_pba,
                                                               Number_of_blocks            &nr_of_pbas)
{
	if (ft_max_lvl_idx >= TREE_MAX_LEVEL) {
		class Program_error_ft_resizing_max_level { };
		throw Program_error_ft_resizing_max_level { };
	}

	ft_max_lvl_idx += 1;

	t1_blks.items[ft_max_lvl_idx] = { };
	t1_blks.items[ft_max_lvl_idx].nodes[0] = ft_root;

	new_pbas.pbas[ft_max_lvl_idx] = alloc_pba_from_resizing_contingent(first_pba, nr_of_pbas);

	ft_root = {
		.pba     = new_pbas.pbas[ft_max_lvl_idx],
		.gen     = curr_gen,
		.hash    = { },
	};

	if (VERBOSE_FT_EXTENSION) {
		log("  set ft root: ", ft_root, " leaves ", ft_nr_of_leaves,
		    " max lvl ", ft_max_lvl_idx);

		log("  set lvl ", ft_max_lvl_idx,
		    " child 0: ", t1_blks.items[ft_max_lvl_idx].nodes[0]);
	}
	(void)ft_nr_of_leaves;
}


void Ft_resizing::_add_new_branch_to_ft_using_pba_contingent(Tree_level_index      const  mount_point_lvl_idx,
                                                             Tree_node_index       const  mount_point_child_idx,
                                                             Tree_degree           const  ft_degree,
                                                             Generation            const  curr_gen,
                                                             Physical_block_address      &first_pba,
                                                             Number_of_blocks            &nr_of_pbas,
                                                             Channel::Type_1_node_blocks &t1_blks,
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
	if (!_generated_prim.succ) {
		error("free tree: request (", _request, ") failed because generated request failed)");
		*(bool *)_request._success_ptr = false;
		_state = COMPLETED;
		return;
	}
	_state = (State)state_uint;
}


void Ft_resizing::_execute_ft_extension_step(Channel        &chan,
                                             unsigned const  chan_idx,
                                             bool           &progress)
{
	Request &req { chan._request };
	switch (chan._state) {
	case Channel::State::SUBMITTED:

		chan._nr_of_leaves = 0;
		chan._vba          = req._ft_nr_of_leaves();

		chan._old_pbas        = { };
		chan._old_generations = { };
		chan._new_pbas        = { };

		chan._lvl_idx                              = req._ft_max_lvl();
		chan._old_pbas.pbas[chan._lvl_idx]         = req._ft_root().pba;
		chan._old_generations.items[chan._lvl_idx] = req._ft_root().gen;

		chan._mt.construct(
			*(Physical_block_address *)req._mt_root_pba_ptr,
			*(Generation *)req._mt_root_gen_ptr,
			*(Hash *)req._mt_root_hash_ptr,
			req._mt_max_level,
			req._mt_degree,
			req._mt_leaves);

		if (chan._vba <= tree_max_max_vba(req._ft_degree, req._ft_max_lvl())) {

			chan._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_FT_RSZG_CACHE,
				.blk_nr = req._ft_root().pba,
				.idx    = chan_idx
			};

			if (VERBOSE_FT_EXTENSION)
				log("  root (", req._ft_root(),
				    " leaves ", req._ft_nr_of_leaves(),
				    " max lvl ", req._ft_max_lvl(),
				    "): load to lvl ", chan._lvl_idx);

			chan._generate_req<Block_io::Read>(Channel::READ_ROOT_NODE_COMPLETED, progress, req._ft_root().pba, chan._encoded_blk);

		} else {

			_add_new_root_lvl_to_ft_using_pba_contingent(req._ft_root(),
			                                             req._ft_max_lvl(),
			                                             req._ft_nr_of_leaves(),
			                                             req._curr_gen,
			                                             chan._t1_blks,
			                                             chan._new_pbas,
			                                             req._pba(),
			                                             req._nr_of_pbas());

			_add_new_branch_to_ft_using_pba_contingent(req._ft_max_lvl(),
			                                           1,
			                                           req._ft_degree,
			                                           req._curr_gen,
			                                           req._pba(),
			                                           req._nr_of_pbas(),
			                                           chan._t1_blks,
			                                           chan._t2_blk,
			                                           chan._new_pbas,
			                                           chan._lvl_idx,
			                                           chan._nr_of_leaves);

			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);

			_set_args_for_write_back_of_inner_lvl(chan, req._ft_max_lvl(),
			                                      chan._lvl_idx,
			                                      chan._new_pbas.pbas[chan._lvl_idx],
			                                      chan_idx,
			                                      chan._state,
			                                      progress,
			                                      chan._generated_prim);
			if (chan._lvl_idx > 1)
				chan._t1_blks.items[chan._lvl_idx].encode_to_blk(chan._encoded_blk);
			else
				chan._t2_blk.encode_to_blk(chan._encoded_blk);

		}

		break;
	case Channel::State::READ_ROOT_NODE_COMPLETED:
		chan._t1_blks.items[chan._lvl_idx].decode_from_blk(chan._encoded_blk);
		_execute_ft_ext_step_read_inner_node_completed(chan, chan_idx, progress);
		break;
	case Channel::State::READ_INNER_NODE_COMPLETED:
		_execute_ft_ext_step_read_inner_node_completed(chan, chan_idx, progress);
		break;
	case Channel::State::ALLOC_PBA_COMPLETED:

		chan._new_pbas.pbas[chan._alloc_lvl_idx] = chan._alloc_pba;
		if (chan._alloc_lvl_idx < req._ft_max_lvl()) {

			chan._alloc_lvl_idx = chan._alloc_lvl_idx + 1;

			if (chan._old_generations.items[chan._alloc_lvl_idx] == req._curr_gen) {

				chan._alloc_pba = chan._old_pbas.pbas[chan._alloc_lvl_idx];
				chan._state = Channel::State::ALLOC_PBA_COMPLETED;
				progress = true;

			} else {

				chan._generated_prim = {
					.op     = Channel::Generated_prim::Type::READ,
					.succ   = false,
					.tg     = Channel::Tag_type::TAG_FT_RSZG_MT_ALLOC,
					.blk_nr = 0,
					.idx    = chan_idx
				};
				chan._alloc_pba = chan._old_pbas.pbas[chan._alloc_lvl_idx];
				chan.generate_req<Meta_tree_request>(
					Channel::ALLOC_PBA_COMPLETED, progress, Meta_tree_request::ALLOC_PBA, *chan._mt,
					req._curr_gen, chan._alloc_pba, chan._generated_prim.succ);
				chan._state = Channel::REQ_GENERATED;
			}

		} else {

			if (VERBOSE_FT_EXTENSION)
				log("  pbas allocated: curr gen ", req._curr_gen);

			_set_args_for_write_back_of_inner_lvl(chan, req._ft_max_lvl(),
			                                      chan._lvl_idx,
			                                      chan._new_pbas.pbas[chan._lvl_idx],
			                                      chan_idx,
			                                      chan._state,
			                                      progress,
			                                      chan._generated_prim);
			if (chan._lvl_idx > 1)
				chan._t1_blks.items[chan._lvl_idx].encode_to_blk(chan._encoded_blk);
			else
				chan._t2_blk.encode_to_blk(chan._encoded_blk);

		}
		break;
	case Channel::State::WRITE_INNER_NODE_COMPLETED:

		if (chan._lvl_idx > 1) {

			Tree_level_index const parent_lvl_idx = chan._lvl_idx + 1;
			Tree_level_index const child_lvl_idx  = chan._lvl_idx;
			Tree_node_index const child_idx = t1_node_idx_for_vba(chan._vba, parent_lvl_idx, req._ft_degree);

			Type_1_node &child {
				chan._t1_blks.items[parent_lvl_idx].nodes[child_idx] };

			child = {
				.pba     = chan._new_pbas.pbas[child_lvl_idx],
				.gen     = req._curr_gen,
				.hash    = { },
			};

			calc_hash(chan._encoded_blk, child.hash);

			if (VERBOSE_FT_EXTENSION)
				log("  set lvl a ", parent_lvl_idx, " child ", child_idx,
				    ": ", child);

			_set_args_for_write_back_of_inner_lvl(chan, req._ft_max_lvl(),
			                                      parent_lvl_idx,
			                                      chan._new_pbas.pbas[parent_lvl_idx],
			                                      chan_idx,
			                                      chan._state,
			                                      progress,
			                                      chan._generated_prim);

			chan._lvl_idx += 1;

		} else {

			Tree_level_index const parent_lvl_idx = chan._lvl_idx + 1;
			Tree_level_index const child_lvl_idx = chan._lvl_idx;
			Tree_node_index const child_idx = t1_node_idx_for_vba(chan._vba, parent_lvl_idx, req._ft_degree);
			Type_1_node &child = chan._t1_blks.items[parent_lvl_idx].nodes[child_idx];
			child = {
				.pba = chan._new_pbas.pbas[child_lvl_idx],
				.gen = req._curr_gen,
			};

			calc_hash(chan._encoded_blk, child.hash);

			if (VERBOSE_FT_EXTENSION)
				log("  set lvl b ", parent_lvl_idx, " child ", child_idx,
				    ": ", child);

			_set_args_for_write_back_of_inner_lvl(chan, req._ft_max_lvl(),
			                                      parent_lvl_idx,
			                                      chan._new_pbas.pbas[parent_lvl_idx],
			                                      chan_idx,
			                                      chan._state,
			                                      progress,
			                                      chan._generated_prim);

			chan._lvl_idx += 1; // = 2

		}


			if (chan._lvl_idx > 1)
				chan._t1_blks.items[chan._lvl_idx].encode_to_blk(chan._encoded_blk);
			else
				chan._t2_blk.encode_to_blk(chan._encoded_blk);
		break;
	case Channel::State::WRITE_ROOT_NODE_COMPLETED: {

		Tree_level_index const child_lvl_idx = chan._lvl_idx;
		Physical_block_address const child_pba = chan._new_pbas.pbas[child_lvl_idx];

		req._ft_root() = {
			.pba = child_pba,
			.gen = req._curr_gen,
		};

		calc_hash(chan._encoded_blk, req._ft_root().hash);

		req._ft_nr_of_leaves() += chan._nr_of_leaves;

		req._success() = true;

		chan._state = Channel::State::COMPLETED;
		progress       = true;

		break;
	}
	default:
		break;
	}
}


void Ft_resizing::execute(bool &progress)
{
	for (unsigned idx = 0; idx < NR_OF_CHANNELS; idx++) {

		Channel &channel = _channels[idx];
		Request &request { channel._request };

		switch (request._type) {
		case Request::INVALID:
			break;
		case Request::FT_EXTENSION_STEP:
			_execute_ft_extension_step(channel, idx, progress);
			break;
		}
	}
}


bool Ft_resizing::_peek_completed_request(uint8_t *buf_ptr,
                                          size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._request._type != Request::INVALID &&
		    channel._state == Channel::COMPLETED) {

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


void Ft_resizing::_drop_completed_request(Module_request &req)
{
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	if (chan._request._type == Request::INVALID ||
	    chan._state != Channel::COMPLETED) {

		class Exception_2 { };
		throw Exception_2 { };
	}
	chan._request._type = Request::INVALID;
}


bool Ft_resizing::_peek_generated_request(uint8_t *buf_ptr,
                                          size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };
		Request &req { chan._request };
		if (req._type == Request::INVALID)
			continue;

		switch (chan._state) {
		case Channel::READ_INNER_NODE_PENDING:

			ASSERT(sizeof(Block_io_request) <= buf_size);
			construct_at<Block_io_request>(
				buf_ptr, FT_RESIZING, id,
				Block_io_request::READ, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, chan._encoded_blk,
				chan._dummy_hash, chan._generated_prim.succ);

			return true;

		case Channel::EXTEND_MT_BY_ONE_LEAF_PENDING:

			class Exception_10 { };
			throw Exception_10 { };

		default: break;
		}
	}
	return false;
}


void Ft_resizing::_drop_generated_request(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._state) {
	case Channel::READ_INNER_NODE_PENDING: chan._state = Channel::READ_INNER_NODE_IN_PROGRESS; break;
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


void Ft_resizing::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case BLOCK_IO:
	{
		switch (chan._state) {
		case Channel::READ_INNER_NODE_IN_PROGRESS:
			if (chan._lvl_idx > 1)
				chan._t1_blks.items[chan._lvl_idx].decode_from_blk(chan._encoded_blk);
			else
				chan._t2_blk.decode_from_blk(chan._encoded_blk);
			chan._state = Channel::READ_INNER_NODE_COMPLETED;
			break;
		default:
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;
	}
	default:
		class Exception_5 { };
		throw Exception_5 { };
	}
}


bool Ft_resizing::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._request._type == Request::INVALID)
			return true;
	}
	return false;
}


void Ft_resizing::submit_request(Module_request &mod_req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		Channel &chan { _channels[id] };
		if (chan._request._type == Request::INVALID) {
			mod_req.dst_request_id(id);
			chan._request = *static_cast<Request *>(&mod_req);
			chan._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}
