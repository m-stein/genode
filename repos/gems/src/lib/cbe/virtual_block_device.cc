/*
 * \brief  Module for virtual block device rekeying
 * \author Martin Stein
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
#include <util/misc_math.h>

/* cbe tester includes */
#include <virtual_block_device.h>
#include <sha256_4k_hash.h>
#include <block_io.h>
#include <crypto.h>
#include <free_tree.h>

using namespace Genode;
using namespace Cbe;


/***************
 ** Utilities **
 ***************/

static void discard_disposable_snapshots(Snapshots  &snapshots,
                                         Generation  curr_gen,
                                         Generation  last_secured_gen)
{
	for (Snapshots_index snap_idx { 0 };
	     snap_idx < MAX_NR_OF_SNAPSHOTS_PER_SB;
	     snap_idx++) {

		Snapshot &snap { snapshots.items[snap_idx] };
		if (snap.valid &&
		    !snap.keep &&
		    snap.gen != curr_gen &&
		    snap.gen != last_secured_gen)
			snap.valid = false;
	}
}


static bool snapshot_contains_vba(Snapshot        const &snap,
                                  Virtual_block_address  vba)
{
	return vba <= snap.nr_of_leaves - 1;
}


static uint64_t log_2(uint64_t const value)
{
	class Log_2_error { };

	if (value == 0)
		throw Log_2_error { };

	uint64_t result = log2(value);
	if (result >= sizeof(value) * 8)
		throw Log_2_error { };

	return result;
}

static Node_index child_idx_for_vba(Virtual_block_address const vba,
                                    Tree_level_index      const lvl,
                                    Tree_degree           const degr)
{
	uint64_t const degree_log_2 { log_2(degr) };
	uint64_t const degree_mask  { ((uint64_t)1 << (uint64_t)degree_log_2) - (uint64_t)1 };
	uint64_t const vba_rshift   { (uint64_t)degree_log_2 * ((uint64_t)lvl - (uint64_t)1) };
	return (uint64_t)degree_mask & (uint64_t)((uint64_t)vba >> (uint64_t)vba_rshift);
}


/**********************************
 ** Virtual_block_device_request **
 **********************************/

char const *Virtual_block_device_request::type_to_string(Type op)
{
	switch (op) {
	case INVALID: return "invalid";
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case REKEY_VBA: return "rekey_vba";
	case VBD_EXTENSION_STEP: return "vbd_extension_step";
	}
	return "?";
}

void Virtual_block_device_request::create(void                  *buf_ptr,
                                          Genode::size_t         buf_size,
                                          Genode::uint64_t       src_module_id,
                                          Genode::uint64_t       src_request_id,
                                          Genode::size_t         req_type,
                                          void                  *prim_ptr,
                                          Genode::size_t         prim_size,
                                          Genode::uint64_t       client_req_offset,
                                          Genode::uint64_t       client_req_tag,
                                          Generation             last_secured_generation,
                                          addr_t                 ft_root_pba_ptr,
                                          addr_t                 ft_root_gen_ptr,
                                          addr_t                 ft_root_hash_ptr,
                                          uint64_t               ft_max_level,
                                          uint64_t               ft_degree,
                                          uint64_t               ft_leaves,
                                          addr_t                 mt_root_pba_ptr,
                                          addr_t                 mt_root_gen_ptr,
                                          addr_t                 mt_root_hash_ptr,
                                          uint64_t               mt_max_level,
                                          uint64_t               mt_degree,
                                          uint64_t               mt_leaves,
                                          uint64_t               vbd_degree,
                                          uint64_t               vbd_highest_vba,
                                          bool                   rekeying,
                                          Virtual_block_address  vba,
                                          Snapshot  const       *snapshot_ptr,
                                          Snapshots const       *snapshots_ptr,
                                          Tree_degree            snapshots_degree,
                                          Key_id                 old_key_id,
                                          Key_id                 new_key_id,
                                          Generation             current_gen,
                                          Key_id                 key_id)
{
	Virtual_block_device_request req { src_module_id, src_request_id };
	req._type                    = (Type)req_type;
	req._last_secured_generation = last_secured_generation;
	req._ft_root_pba_ptr         = (addr_t)ft_root_pba_ptr;
	req._ft_root_gen_ptr         = (addr_t)ft_root_gen_ptr;
	req._ft_root_hash_ptr        = (addr_t)ft_root_hash_ptr;
	req._ft_max_level            = ft_max_level;
	req._ft_degree               = ft_degree;
	req._ft_leaves               = ft_leaves;
	req._mt_root_pba_ptr         = (addr_t)mt_root_pba_ptr;
	req._mt_root_gen_ptr         = (addr_t)mt_root_gen_ptr;
	req._mt_root_hash_ptr        = (addr_t)mt_root_hash_ptr;
	req._mt_max_level            = mt_max_level;
	req._mt_degree               = mt_degree;
	req._mt_leaves               = mt_leaves;
	req._vbd_degree              = vbd_degree;
	req._vbd_highest_vba         = vbd_highest_vba;
	req._rekeying                = rekeying;
	req._vba                     = vba;
	switch (req_type) {
	case READ_VBA:
	case WRITE_VBA:
		req._snapshots.items[0] = *snapshot_ptr;
		req._new_key_id         = key_id;
		break;
	case REKEY_VBA:
		req._snapshots = *snapshots_ptr;
		req._old_key_id = old_key_id;
		req._new_key_id = new_key_id;
		break;
	default:
		class Exception_3 { };
		throw Exception_3 { };
	}
	req._snapshots_degree        = snapshots_degree;
	req._client_req_offset       = client_req_offset;
	req._client_req_tag          = client_req_tag;
	req._curr_gen                = current_gen;

	if (prim_ptr != nullptr) {
		if (prim_size > sizeof(req._prim)) {
			class Exception_1 { };
			throw Exception_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
	}
	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Virtual_block_device_request::Virtual_block_device_request(unsigned long src_module_id,
                                                           unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, VIRTUAL_BLOCK_DEVICE }
{ }


/**************************
 ** Virtual_block_device **
 **************************/

void Virtual_block_device::_set_args_for_write_back_of_t1_lvl(Tree_level_index const max_lvl,
                                                              uint64_t const  t1_lvl,
                                                              uint64_t const  pba,
                                                              uint64_t const  prim_idx,
                                                              Channel::State &state,
                                                              bool &progress,
                                                              Channel::Generated_prim &prim)
{
	prim = {
		.op     = Channel::Generated_prim::Type::WRITE,
		.succ   = false,
		.tg     = Channel::Tag_type::TAG_VBD_CACHE,
		.blk_nr = pba,
		.idx    = prim_idx
	};

	if (t1_lvl < max_lvl) {
		state = Channel::State::WRITE_INNER_NODE_PENDING;
		progress = true;
	} else {
		state = Channel::State::WRITE_ROOT_NODE_PENDING;
		progress = true;
	}
}


bool Virtual_block_device::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._request._type == Request::INVALID)
			return true;
	}
	return false;
}


void Virtual_block_device::submit_request(Module_request &mod_req)
{
	for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
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


void Virtual_block_device::_execute_read_vba_read_inner_node_completed (Channel        &channel,
                                                                        uint64_t const  job_idx,
                                                                        bool           &progress)
{
	_check_that_primitive_was_successful(channel._generated_prim);

	auto &snapshot = channel.snapshots(channel._snapshot_idx);

	_check_hash_of_read_type_1_node(snapshot,
	                                channel._request._snapshots_degree,
	                                channel._t1_blk_idx, channel._t1_blks,
	                                channel._vba);

	if (channel._t1_blk_idx > 1) {

		auto const parent_lvl = channel._t1_blk_idx;
		auto const child_lvl  = channel._t1_blk_idx - 1;

		auto const  child_idx = child_idx_for_vba(channel._request._vba, parent_lvl, channel._request._snapshots_degree);
		auto const &child     = channel._t1_blks.items[parent_lvl].nodes[child_idx];

		channel._t1_blk_idx = child_lvl;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_CACHE,
			.blk_nr = child.pba,
			.idx    = job_idx
		};
		if (VERBOSE_VBA_ACCESS) {
			log(
				"  lvl ", channel._t1_blk_idx,
				": read t1 nodes blk: pba ", channel._generated_prim.blk_nr);
		}

		channel._state = Channel::State::READ_INNER_NODE_PENDING;
		progress = true;

	} else {

		Tree_level_index const parent_lvl { channel._t1_blk_idx };
		Node_index const child_idx {
			child_idx_for_vba(
				channel._request._vba, parent_lvl,
				channel._request._snapshots_degree) };

		Type_1_node const &child {
			channel._t1_blks.items[parent_lvl].nodes[child_idx] };

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_BLK_IO_READ_CLIENT_DATA,
			.blk_nr = child.pba,
			.idx    = job_idx
		};
		if (VERBOSE_VBA_ACCESS) {
			log(
				"  lvl ", 0,
				": read data blk: pba ", channel._generated_prim.blk_nr,
				" key ", channel._request._new_key_id);
		}

		channel._state = Channel::State::READ_CLIENT_DATA_FROM_LEAF_NODE_PENDING;
		progress       = true;
	}
}


void Virtual_block_device::_execute_read_vba(Channel &channel,
                                             uint64_t const idx,
                                             bool &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
	{
		Request &request  = channel._request;

		channel._snapshot_idx = 0;
		channel._vba          = request._vba;

		Snapshot &snapshot = channel.snapshots(channel._snapshot_idx);
		channel._t1_blk_idx = snapshot.max_level;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_CACHE,
			.blk_nr = snapshot.pba,
			.idx    = idx
		};
		if (VERBOSE_VBA_ACCESS)
			log("  lvl ", channel._t1_blk_idx, ": read t1 nodes blk: pba ",
			    channel._generated_prim.blk_nr);

		channel._state = Channel::State::READ_ROOT_NODE_PENDING;
		progress       = true;
		break;
	}
	case Channel::State::READ_ROOT_NODE_COMPLETED:

		_execute_read_vba_read_inner_node_completed (channel, idx, progress);
		break;

	case Channel::State::READ_INNER_NODE_COMPLETED:

		_execute_read_vba_read_inner_node_completed (channel, idx, progress);
		break;

	case Channel::State::READ_CLIENT_DATA_FROM_LEAF_NODE_COMPLETED:

		_check_that_primitive_was_successful(channel._generated_prim);
		channel._request._success = channel._generated_prim.succ;
		channel._state            = Channel::State::COMPLETED;
		progress                  = true;
		break;
	default:
		break;
	}
}


void Virtual_block_device::_update_nodes_of_branch_of_written_vba(Snapshot &snapshot,
                                                                  uint64_t const snapshot_degree,
                                                                  uint64_t const vba,
                                                                  Tree_walk_pbas const &new_pbas,
                                                                  Hash_new const & leaf_hash,
                                                                  uint64_t curr_gen,
                                                                  Channel::Type_1_node_blocks &t1_blks)
{
	for (Tree_level_index lvl = 0; lvl <= snapshot.max_level; lvl++) {

		if (lvl == 0) {

			auto const  child_idx = child_idx_for_vba(vba, lvl + 1, snapshot_degree);
			auto       &node      = t1_blks.items[lvl + 1].nodes[child_idx];

			node.pba  = new_pbas.pbas[lvl];
			node.gen  = curr_gen;
			node.hash = leaf_hash;

		} else if (lvl < snapshot.max_level) {

			auto const  child_idx = child_idx_for_vba(vba, lvl + 1, snapshot_degree);
			auto       &node      = t1_blks.items[lvl + 1].nodes[child_idx];

			node.pba   = new_pbas.pbas[lvl];
			node.gen   = curr_gen;
			calc_sha256_4k_hash(&t1_blks.items[lvl], &node.hash);

		} else {

			snapshot.pba   = new_pbas.pbas[lvl];
			snapshot.gen   = curr_gen;
			calc_sha256_4k_hash(&t1_blks.items[lvl], &snapshot.hash);
		}
	}
}


void Virtual_block_device::
_set_args_in_order_to_write_client_data_to_leaf_node(Tree_walk_pbas const &new_pbas,
                                                     uint64_t                const  job_idx,
                                                     Channel::State                &state,
                                                     Channel::Generated_prim       &prim,
                                                     bool                          &progress)
{
	prim = {
		.op     = Channel::Generated_prim::Type::WRITE,
		.succ   = false,
		.tg     = Channel::Tag_type::TAG_VBD_BLK_IO_WRITE_CLIENT_DATA,
		.blk_nr = new_pbas.pbas[0],
		.idx    = job_idx
	};

	state    = Channel::State::WRITE_CLIENT_DATA_TO_LEAF_NODE_PENDING;
	progress = true;
}


void Virtual_block_device::
_check_that_primitive_was_successful(Channel::Generated_prim const &prim)
{
	if (prim.succ)
		return;

	class Primitive_not_successfull { };
	throw Primitive_not_successfull { };
}


void Virtual_block_device::_check_hash_of_read_type_1_node(Snapshot const &snapshot,
                                                           uint64_t const snapshots_degree,
                                                           uint64_t const t1_blk_idx,
                                                           Channel::Type_1_node_blocks const &t1_blks,
                                                           uint64_t const vba)
{
	if (t1_blk_idx == snapshot.max_level) {
		if (!check_sha256_4k_hash(&t1_blks.items[t1_blk_idx], &snapshot.hash)) {
			class Program_error_hash_of_read_type_1 { };
			throw Program_error_hash_of_read_type_1 { };
		}
	} else {
		uint64_t    const  child_idx = child_idx_for_vba(vba, t1_blk_idx + 1, snapshots_degree);
		Type_1_node const &child     = t1_blks.items[t1_blk_idx + 1].nodes[child_idx];
		if (!check_sha256_4k_hash(&t1_blks.items[t1_blk_idx], &child.hash)) {
			class Program_error_hash_of_read_type_1_B { };
			throw Program_error_hash_of_read_type_1_B { };
		}
	}
}


void Virtual_block_device::_set_args_in_order_to_read_type_1_node(Snapshot const &snapshot,
                                                                  uint64_t const snapshots_degree,
                                                                  uint64_t const t1_blk_idx,
                                                                  Channel::Type_1_node_blocks const &t1_blks,
                                                                  uint64_t const vba,
                                                                  uint64_t const job_idx,
                                                                  Channel::State &state,
                                                                  Channel::Generated_prim &prim,
                                                                  bool &progress)
{
	if (t1_blk_idx == snapshot.max_level) {
		prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_CACHE,
			.blk_nr = snapshot.pba,
			.idx    = job_idx
		};
	} else {
		auto const  child_idx = child_idx_for_vba(vba, t1_blk_idx + 1, snapshots_degree);
		auto const &child     = t1_blks.items[t1_blk_idx + 1].nodes[child_idx];

		prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_CACHE,
			.blk_nr = child.pba,
			.idx    = job_idx
		};
	}

	state = Channel::State::READ_INNER_NODE_PENDING;
	progress = true;
}


void Virtual_block_device::
_initialize_new_pbas_and_determine_nr_of_pbas_to_allocate(uint64_t                    const  curr_gen,
                                                          Snapshot                    const &snapshot,
                                                          uint64_t                    const  snapshots_degree,
                                                          uint64_t                    const  vba,
                                                          Channel::Type_1_node_blocks const &t1_blks,
                                                          Tree_walk_pbas                    &new_pbas,
                                                          uint64_t                          &nr_of_blks)
{
	nr_of_blks = 0;
	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {

		if (lvl > snapshot.max_level) {

			new_pbas.pbas[lvl] = 0;

		} else if (lvl == snapshot.max_level) {

			if (snapshot.gen < curr_gen) {

				nr_of_blks++;
				new_pbas.pbas[lvl] = 0;

			} else if (snapshot.gen == curr_gen) {

				new_pbas.pbas[lvl] = snapshot.pba;

			} else {

				class Exception_1 { };
				throw Exception_1 { };
			}
		} else {

			Node_index const child_idx {
				child_idx_for_vba(vba, lvl + 1, snapshots_degree) };

			Type_1_node const &child {
				t1_blks.items[lvl + 1].nodes[child_idx] };

			if (child.gen < curr_gen) {

				if (lvl == 0 && child.gen == INVALID_GENERATION) {

					new_pbas.pbas[lvl] = child.pba;

				} else {

					nr_of_blks++;
					new_pbas.pbas[lvl] = 0;
				}
			} else if (child.gen == curr_gen) {

				new_pbas.pbas[lvl] = child.pba;

			} else {

				class Exception_2 { };
				throw Exception_2 { };
			}
		}
	}
}


void Virtual_block_device::
_set_args_for_alloc_of_new_pbas_for_branch_of_written_vba(uint64_t curr_gen,
                                                          Snapshot const &snapshot,
                                                          uint64_t const snapshots_degree,
                                                          uint64_t const vba,
                                                          Channel::Type_1_node_blocks const &t1_blks,
                                                          uint64_t const prim_idx,
                                                          uint64_t                 &free_gen,
                                                          Type_1_node_walk         &t1_walk,
                                                          Channel::State           &state,
                                                          Channel::Generated_prim  &prim,
                                                          bool                     &progress)
{
	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {
		if (lvl > snapshot.max_level) {
			t1_walk.nodes[lvl] = Type_1_node_unpadded { }; /* invalid */
		} else if (lvl == snapshot.max_level) {
			auto &node = t1_walk.nodes[lvl];

			node.pba  = snapshot.pba;
			node.gen  = snapshot.gen;
			node.hash = snapshot.hash;
		} else {
			auto const   child_idx = child_idx_for_vba(vba, lvl + 1, snapshots_degree);
			t1_walk.nodes[lvl] = *(Type_1_node_unpadded*)&t1_blks.items[lvl + 1].nodes[child_idx];
		}
	}

	free_gen = curr_gen;

	prim = {
		.op     = Channel::Generated_prim::Type::READ,
		.succ   = false,
		.tg     = Channel::Tag_type::TAG_VBD_FT_ALLOC_FOR_NON_RKG,
		.blk_nr = 0,
		.idx    = prim_idx
	};

	state = Channel::State::ALLOC_PBAS_AT_LEAF_LVL_PENDING;
	progress = true;
}


void Virtual_block_device::_execute_write_vba(Channel        &chan,
                                              uint64_t const  job_idx,
                                              bool           &progress)
{
	Request &req { chan._request };
	switch (chan._state) {
	case Channel::State::SUBMITTED: {
		Request &request { chan._request };

		chan._snapshot_idx = 0;
		chan._vba = request._vba;
		chan._t1_blk_idx = chan.snapshots(chan._snapshot_idx).max_level;

		_set_args_in_order_to_read_type_1_node(chan.snapshots(chan._snapshot_idx),
		                                       chan._request._snapshots_degree,
		                                       chan._t1_blk_idx,
		                                       chan._t1_blks,
		                                       chan._vba,
		                                       job_idx,
		                                       chan._state,
		                                       chan._generated_prim,
		                                       progress);

		if (VERBOSE_VBA_ACCESS) {
			log("  lvl ", chan._t1_blk_idx, ": read t1 nodes blk: pba ",
			    chan._generated_prim.blk_nr);
		}

		break;
	}
	case Channel::State::READ_INNER_NODE_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);
		_check_hash_of_read_type_1_node(chan.snapshots(chan._snapshot_idx),
		                                chan._request._snapshots_degree,
		                                chan._t1_blk_idx, chan._t1_blks,
		                                chan._vba);

		if (chan._t1_blk_idx > 1) {
			chan._t1_blk_idx = chan._t1_blk_idx - 1;

			_set_args_in_order_to_read_type_1_node(chan.snapshots(chan._snapshot_idx),
			                                       chan._request._snapshots_degree,
			                                       chan._t1_blk_idx,
			                                       chan._t1_blks,
			                                       chan._vba,
			                                       job_idx,
			                                       chan._state,
			                                       chan._generated_prim,
			                                       progress);

			if (VERBOSE_VBA_ACCESS) {
				log("  lvl ", chan._t1_blk_idx, ": read t1 nodes blk: pba ",
				    chan._generated_prim.blk_nr);
			}

		} else {
			_initialize_new_pbas_and_determine_nr_of_pbas_to_allocate(req._curr_gen,
			                                                          chan.snapshots(chan._snapshot_idx),
			                                                          chan._request._snapshots_degree,
			                                                          chan._vba,
			                                                          chan._t1_blks,
			                                                          chan._new_pbas,
			                                                          chan._nr_of_blks);

			if (chan._nr_of_blks > 0) {
				_set_args_for_alloc_of_new_pbas_for_branch_of_written_vba(req._curr_gen,
				                                                          chan.snapshots(chan._snapshot_idx),
				                                                          chan._request._snapshots_degree,
				                                                          chan._vba,
				                                                          chan._t1_blks,
				                                                          job_idx,
				                                                          chan._free_gen,
				                                                          chan._t1_node_walk,
				                                                          chan._state,
				                                                          chan._generated_prim,
				                                                          progress);

			} else {

				_set_args_in_order_to_write_client_data_to_leaf_node(chan._new_pbas,
				                                                     job_idx,
				                                                     chan._state,
				                                                     chan._generated_prim,
				                                                     progress);

				if (VERBOSE_VBA_ACCESS)
					log("  lvl ", 0, ": write data blk: pba ",
					    chan._generated_prim.blk_nr,
					    " key ", req._new_key_id);
			}
		}

		break;
	case Channel::State::ALLOC_PBAS_AT_LEAF_LVL_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);

		if (VERBOSE_VBA_ACCESS) {
			log("  lvl ", 0, ": alloc ", chan._nr_of_blks, " pba",
			    chan._nr_of_blks > 1 ? "s" : "");

			for (Tree_level_index lvl = TREE_MAX_LEVEL; ; lvl--) {
				if (lvl <= chan.snapshots(chan._snapshot_idx).max_level) {
					Type_1_node_unpadded &node = chan._t1_node_walk.nodes[lvl];
					log("    lvl ", lvl, " gen ", (Generation)node.gen,
					    " pba ", (Physical_block_address)node.pba, " -> ",
					    (Physical_block_address)chan._new_pbas.pbas[lvl]);
				}
				if (lvl == 0)
					break;
			}
		}
		_set_args_in_order_to_write_client_data_to_leaf_node(chan._new_pbas,
		                                                     job_idx,
		                                                     chan._state,
		                                                     chan._generated_prim,
		                                                     progress);
		if (VERBOSE_VBA_ACCESS)
			log("  lvl ", 0, ": write data blk: pba ",
			    chan._generated_prim.blk_nr,
			    " key ", req._new_key_id);

		break;

	case Channel::State::WRITE_CLIENT_DATA_TO_LEAF_NODE_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);
		_update_nodes_of_branch_of_written_vba(
			chan.snapshots(chan._snapshot_idx),
			chan._request._snapshots_degree, chan._vba,
			chan._new_pbas, chan._hash, req._curr_gen, chan._t1_blks);

		_set_args_for_write_back_of_t1_lvl(
			chan.snapshots(chan._snapshot_idx).max_level, chan._t1_blk_idx,
			chan._new_pbas.pbas[chan._t1_blk_idx], job_idx, chan._state,
			progress, chan._generated_prim);

		if (VERBOSE_VBA_ACCESS)
			log("  lvl ", chan._t1_blk_idx, ": write t1 nodes blk: pba ",
			    chan._generated_prim.blk_nr);

		break;

	case Channel::State::WRITE_INNER_NODE_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);
		chan._t1_blk_idx = chan._t1_blk_idx + 1;

		_set_args_for_write_back_of_t1_lvl(
			chan.snapshots(chan._snapshot_idx).max_level,
			chan._t1_blk_idx, chan._new_pbas.pbas[chan._t1_blk_idx],
			job_idx, chan._state, progress, chan._generated_prim);

		if (VERBOSE_VBA_ACCESS)
			log("  lvl ", chan._t1_blk_idx, ": write t1 nodes blk: pba ",
				chan._generated_prim.blk_nr);

		break;
	case Channel::State::WRITE_ROOT_NODE_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);
		chan._state = Channel::State::COMPLETED;
		chan._request._success = true;
		progress = true;
		break;

	default:
		break;
	}
}


void Virtual_block_device::_mark_req_failed(Channel    &chan,
                                            bool       &progress,
                                            char const *str)
{
	error(chan._request.type_name(), " request failed at step \"", str, "\"");
	chan._request._success = false;
	chan._state = Channel::COMPLETED;
	progress = true;
}


void Virtual_block_device::_mark_req_successful(Channel &chan,
                                                bool    &progress)
{
	chan._request._success = true;
	chan._state = Channel::COMPLETED;
	progress = true;
}


char const *Virtual_block_device::_state_to_step_label(Channel::State state)
{
	switch (state) {
	case Channel::READ_ROOT_NODE_COMPLETED: return "read root node";
	case Channel::READ_INNER_NODE_COMPLETED: return "read inner node";
	case Channel::READ_LEAF_NODE_COMPLETED: return "read leaf node";
	case Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_COMPLETED: return "read client data from leaf node";
	case Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_COMPLETED: return "write client data to leaf node";
	case Channel::DECRYPT_LEAF_NODE_COMPLETED: return "decrypt leaf node";
	case Channel::ALLOC_PBAS_AT_LEAF_LVL_COMPLETED: return "alloc pbas at leaf lvl";
	case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_COMPLETED: return "alloc pbas at lowest inner lvl";
	case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_COMPLETED: return "alloc pbas at higher inner lvl";
	case Channel::ENCRYPT_LEAF_NODE_COMPLETED: return "encrypt leaf node";
	case Channel::WRITE_LEAF_NODE_COMPLETED: return "write leaf node";
	case Channel::WRITE_INNER_NODE_COMPLETED: return "write inner node";
	case Channel::WRITE_ROOT_NODE_COMPLETED: return "write root node";
	default: break;
	}
	return "?";
}


bool Virtual_block_device::_handle_failed_generated_req(Channel &chan,
                                                        bool    &progress)
{
	if (chan._generated_prim.succ)
		return false;

	_mark_req_failed(chan, progress, _state_to_step_label(chan._state));
	return true;
}


bool
Virtual_block_device::_find_next_snap_to_rekey_vba_at(Channel const   &chan,
                                                      Snapshots_index &next_snap_idx)
{
	bool next_snap_idx_valid { false };
	Request const &req { chan._request };
	Snapshot const &old_snap { req._snapshots.items[chan._snapshot_idx] };

	for (Snapshots_index snap_idx { 0 };
	     snap_idx < MAX_NR_OF_SNAPSHOTS_PER_SB;
	     snap_idx++) {

		Snapshot const &snap { req._snapshots.items[snap_idx] };
		if (snap.valid && snapshot_contains_vba(snap, req._vba)) {

			if (next_snap_idx_valid) {

				Snapshot const &next_snap { req._snapshots.items[next_snap_idx] };
				if (snap.gen > next_snap.gen &&
				    snap.gen < old_snap.gen)
					next_snap_idx = snap_idx;

			} else {

				if (snap.gen < old_snap.gen) {

					next_snap_idx = snap_idx;
					next_snap_idx_valid = true;
				}
			}
		}
	}
	return next_snap_idx_valid;
}


void Virtual_block_device::
_set_args_for_alloc_of_new_pbas_for_rekeying(Channel                  &chan,
                                             uint64_t                  chan_idx,
                                             Type_1_node_blocks_index  min_lvl)
{
	bool const for_curr_gen_blks { chan._first_snapshot };
	Generation const curr_gen { chan._request._curr_gen };
	Snapshot const &snap { chan._request._snapshots.items[chan._snapshot_idx] };
	Tree_degree const snap_degree { chan._request._snapshots_degree };
	Virtual_block_address const vba { chan._request._vba };
	Type_1_node_blocks const &t1_blks { chan._t1_blks };
	Type_1_node_walk &t1_walk { chan._t1_node_walk };
	Tree_walk_pbas &new_pbas { chan._new_pbas };

	if (min_lvl > snap.max_level) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	chan._nr_of_blks = 0;

	if (for_curr_gen_blks)
		chan._free_gen = curr_gen;
	else
		chan._free_gen = snap.gen + 1;

	for (Tree_level_index lvl = 0; lvl <= TREE_MAX_LEVEL; lvl++) {

		if (lvl > snap.max_level) {

			t1_walk.nodes[lvl] = { };
			new_pbas.pbas[lvl] = 0;

		} else if (lvl == snap.max_level) {

			chan._nr_of_blks++;
			new_pbas.pbas[lvl] = 0;
			t1_walk.nodes[lvl] = { snap.pba, snap.gen, snap.hash };

		} else if (lvl >= min_lvl) {

			chan._nr_of_blks++;
			new_pbas.pbas[lvl] = 0;
			Node_index const child_idx {
				child_idx_for_vba(vba, lvl + 1, snap_degree) };

			t1_walk.nodes[lvl] = t1_blks.items[lvl + 1].nodes[child_idx];

		} else {

			Node_index const child_idx {
				child_idx_for_vba(vba, lvl + 1, snap_degree) };

			Type_1_node const &child { t1_blks.items[lvl + 1].nodes[child_idx] };
			t1_walk.nodes[lvl] = { new_pbas.pbas[lvl], child.gen, child.hash};
		}
	}
	if (for_curr_gen_blks) {

		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_VBD_FT_ALLOC_FOR_RKG_CURR_GEN_BLKS,
			.blk_nr = 0,
			.idx    = chan_idx
		};

	} else {

		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_VBD_FT_ALLOC_FOR_RKG_OLD_GEN_BLKS,
			.blk_nr = 0,
			.idx    = chan_idx
		};
	}
}


void Virtual_block_device::_execute_rekey_vba(Channel  &chan,
                                              uint64_t  chan_idx,
                                              bool     &progress)
{
	Request &req { chan._request };
	switch (chan._state) {
	case Channel::State::SUBMITTED:
	{
		discard_disposable_snapshots(
			req._snapshots, req._curr_gen, req._last_secured_generation);

		Snapshots_index first_snap_idx { 0 };
		bool first_snap_idx_found { false };
		for (Snapshots_index snap_idx { 0 };
		     snap_idx < MAX_NR_OF_SNAPSHOTS_PER_SB;
		     snap_idx++) {

			Snapshot const &snap { req._snapshots.items[snap_idx] };
			Snapshot const &first_snap { req._snapshots.items[first_snap_idx] };
			if (snap.valid &&
			    (!first_snap_idx_found || snap.gen > first_snap.gen)) {

				first_snap_idx = snap_idx;
				first_snap_idx_found = true;
			}
		}
		if (!first_snap_idx_found) {

			class Exception_1 { };
			throw Exception_1 { };
		}
		chan._snapshot_idx = first_snap_idx;
		chan._first_snapshot = true;

		Snapshot const &snap { req._snapshots.items[chan._snapshot_idx] };
		chan._t1_blk_idx = snap.max_level;
		chan._t1_blks_old_pbas.items[chan._t1_blk_idx] = snap.pba;

		if (VERBOSE_REKEYING) {
			log("    snapshot ", chan._snapshot_idx, ":");
			log("      lvl ", (Tree_level_index)snap.max_level + 1,
			    ": old snap: ", snap);
		}
		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_VBD_CACHE,
			.blk_nr = snap.pba,
			.idx    = chan_idx
		};
		chan._state = Channel::READ_ROOT_NODE_PENDING;
		progress = true;

		break;
	}
	case Channel::READ_ROOT_NODE_COMPLETED:
	case Channel::READ_INNER_NODE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		Snapshot const &snap { req._snapshots.items[chan._snapshot_idx] };
		if (chan._t1_blk_idx == snap.max_level) {

			if (!check_sha256_4k_hash(&chan._t1_blks.items[chan._t1_blk_idx], &snap.hash)) {

				_mark_req_failed(chan, progress, "check root node hash");
				break;
			}

		} else {

			Type_1_node_blocks_index const parent_lvl { chan._t1_blk_idx + 1 };
			Type_1_node_block_index const child_idx {
				child_idx_for_vba(req._vba, parent_lvl, req._snapshots_degree) };

			if (!check_sha256_4k_hash(&chan._t1_blks.items[chan._t1_blk_idx],
			                          &chan._t1_blks.items[parent_lvl].nodes[child_idx].hash)) {

				_mark_req_failed(chan, progress, "check inner node hash");
				break;
			}
		}
		if (chan._t1_blk_idx > 1) {

			Type_1_node_blocks_index const parent_lvl { chan._t1_blk_idx };
			Type_1_node_blocks_index const child_lvl { parent_lvl - 1 };
			Type_1_node_block_index const child_idx {
				child_idx_for_vba(req._vba, parent_lvl, req._snapshots_degree) };

			Type_1_node const &child { chan._t1_blks.items[parent_lvl].nodes[child_idx] };

			if (VERBOSE_REKEYING)
				log("      lvl ", parent_lvl, ": old t1 node ", child_idx,
				    ": ", child);

			if (!chan._first_snapshot &&
			    chan._t1_blks_old_pbas.items[child_lvl] == child.pba) {

				/*
				 * The rest of this branch has already been rekeyed while
				 * rekeying the vba at another snapshot and can therefore be
				 * skipped.
				 */
				chan._t1_blk_idx = child_lvl;
				_set_args_for_alloc_of_new_pbas_for_rekeying(chan, chan_idx, parent_lvl);
				chan._state = Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_PENDING;
				progress = true;

			} else {

				chan._t1_blk_idx = child_lvl;
				chan._t1_blks_old_pbas.items[child_lvl] = child.pba;
				chan._generated_prim = {
					.op     = Generated_prim::READ,
					.succ   = false,
					.tg     = Channel::TAG_VBD_CACHE,
					.blk_nr = child.pba,
					.idx    = chan_idx
				};
				chan._state = Channel::READ_INNER_NODE_PENDING;
				progress = true;
			}

		} else {

			Type_1_node_blocks_index const parent_lvl { chan._t1_blk_idx };
			Type_1_node_block_index const child_idx {
				child_idx_for_vba(req._vba, parent_lvl, req._snapshots_degree) };

			Type_1_node const &child { chan._t1_blks.items[parent_lvl].nodes[child_idx] };

			if (VERBOSE_REKEYING)
				log("      lvl ", parent_lvl, ": old t1 node ", child_idx,
				    ": ", child);

			if (!chan._first_snapshot
			    && chan._data_blk_old_pba == child.pba) {

				/*
				 * The leaf node of this branch has already been rekeyed while
				 * rekeying the vba at another snapshot and can therefore be
				 * skipped.
				 */
				_set_args_for_alloc_of_new_pbas_for_rekeying(
					chan, chan_idx, parent_lvl);

				chan._state = Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_PENDING;
				progress = true;

			} else if (child.gen == INITIAL_GENERATION) {

				/*
				 * The leaf node of this branch is still unused and can
				 * therefore be skipped because the driver will yield all
				 * zeroes for it regardless of the used key.
				 */
				_set_args_for_alloc_of_new_pbas_for_rekeying(chan, chan_idx, 0);
				chan._state = Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_PENDING;
				progress = true;

			} else {

				chan._data_blk_old_pba = child.pba;
				chan._generated_prim = {
					.op     = Generated_prim::READ,
					.succ   = false,
					.tg     = Channel::TAG_VBD_BLK_IO,
					.blk_nr = child.pba,
					.idx    = chan_idx
				};
				chan._state = Channel::READ_LEAF_NODE_PENDING;
				progress = true;
			}
		}
		break;
	}
	case Channel::READ_LEAF_NODE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		Type_1_node_blocks_index const parent_lvl { FIRST_T1_NODE_BLKS_IDX };
		Type_1_node_block_index const child_idx {
			child_idx_for_vba(req._vba, parent_lvl, req._snapshots_degree) };

		Type_1_node &node {
			chan._t1_blks.items[parent_lvl].nodes[child_idx] };

		if (!check_sha256_4k_hash(&chan._data_blk, &node.hash)) {

			_mark_req_failed(chan, progress, "check leaf node hash");
			break;
		}
		chan._generated_prim = {
			.op     = Generated_prim::READ,
			.succ   = false,
			.tg     = Channel::TAG_VBD_CRYPTO_DECRYPT,
			.blk_nr = chan._data_blk_old_pba,
			.idx    = chan_idx
		};
		chan._state = Channel::DECRYPT_LEAF_NODE_PENDING;
		progress = true;

		if (VERBOSE_REKEYING)
			log("      lvl 0: old data: cipher ", chan._data_blk);

		break;
	}
	case Channel::DECRYPT_LEAF_NODE_COMPLETED:

		if (_handle_failed_generated_req(chan, progress))
			break;

		if (VERBOSE_REKEYING)
			log("                       plain  ", chan._data_blk);

		_set_args_for_alloc_of_new_pbas_for_rekeying(chan, chan_idx, 0);
		chan._state = Channel::ALLOC_PBAS_AT_LEAF_LVL_PENDING;
		progress = true;
		break;

	case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_COMPLETED:

		if (_handle_failed_generated_req(chan, progress))
			break;

		chan._state = Channel::WRITE_LEAF_NODE_COMPLETED;
		progress = true;
		break;

	case Channel::ALLOC_PBAS_AT_LEAF_LVL_COMPLETED:

		if (_handle_failed_generated_req(chan, progress))
			break;

		chan._generated_prim = {
			.op     = Generated_prim::WRITE,
			.succ   = false,
			.tg     = Channel::TAG_VBD_CRYPTO_ENCRYPT,
			.blk_nr = chan._new_pbas.pbas[0],
			.idx    = chan_idx
		};
		chan._state = Channel::ENCRYPT_LEAF_NODE_PENDING;
		progress = true;

		if (VERBOSE_REKEYING)
			log("      lvl 0: new data: plain  ", chan._data_blk);

		break;

	case Channel::ENCRYPT_LEAF_NODE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		Tree_level_index const child_lvl { 0 };
		Physical_block_address const child_pba {
			chan._new_pbas.pbas[child_lvl] };

		chan._generated_prim = {
			.op     = Generated_prim::WRITE,
			.succ   = false,
			.tg     = Channel::TAG_VBD_BLK_IO,
			.blk_nr = child_pba,
			.idx    = chan_idx
		};
		chan._state = Channel::WRITE_LEAF_NODE_PENDING;
		progress = true;

		if (VERBOSE_REKEYING)
			log("                       cipher ", chan._data_blk);

		break;
	}
	case Channel::WRITE_LEAF_NODE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		Type_1_node_blocks_index const parent_lvl { 1 };
		Tree_level_index         const child_lvl  { 0 };
		Physical_block_address   const child_pba  { chan._new_pbas.pbas[child_lvl] };
		Physical_block_address   const parent_pba { chan._new_pbas.pbas[parent_lvl] };
		Type_1_node_block_index  const child_idx  {
			child_idx_for_vba(req._vba, parent_lvl, req._snapshots_degree) };

		Type_1_node &node { chan._t1_blks.items[parent_lvl].nodes[child_idx] };
		node.pba = child_pba;
		calc_sha256_4k_hash(&chan._data_blk, &node.hash);

		if (VERBOSE_REKEYING)
			log("      lvl ", parent_lvl,
			    ": new t1 node ", child_idx, ": ", node);

		chan._generated_prim = {
			.op     = Generated_prim::WRITE,
			.succ   = false,
			.tg     = Channel::TAG_VBD_CACHE,
			.blk_nr = parent_pba,
			.idx    = chan_idx
		};
		chan._state = Channel::WRITE_INNER_NODE_PENDING;
		progress = true;
		break;
	}
	case Channel::WRITE_INNER_NODE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		Snapshot                 const &snap       { req._snapshots.items[chan._snapshot_idx] };
		Type_1_node_blocks_index const  parent_lvl { chan._t1_blk_idx + 1 };
		Tree_level_index         const  child_lvl  { chan._t1_blk_idx };
		Physical_block_address   const  child_pba  { chan._new_pbas.pbas[child_lvl] };
		Physical_block_address   const  parent_pba { chan._new_pbas.pbas[parent_lvl] };
		Type_1_node_block_index  const  child_idx  {
			child_idx_for_vba(req._vba, parent_lvl, req._snapshots_degree) };;

		Type_1_node &node { chan._t1_blks.items[parent_lvl].nodes[child_idx] };
		node.pba = child_pba;
		calc_sha256_4k_hash(&chan._t1_blks.items[child_lvl], &node.hash);

		if (VERBOSE_REKEYING)
			log("      lvl ", parent_lvl,
			    ": new t1 node ", child_idx, ": ", node);

		chan._t1_blk_idx++;
		chan._generated_prim = {
			.op     = Generated_prim::WRITE,
			.succ   = false,
			.tg     = Channel::TAG_VBD_CACHE,
			.blk_nr = parent_pba,
			.idx    = chan_idx
		};
		if (chan._t1_blk_idx < snap.max_level)
			chan._state = Channel::WRITE_INNER_NODE_PENDING;
		else
			chan._state = Channel::WRITE_ROOT_NODE_PENDING;

		progress = true;
		break;
	}
	case Channel::WRITE_ROOT_NODE_COMPLETED:
	{
		if (_handle_failed_generated_req(chan, progress))
			break;

		Snapshot                     &snap      { req._snapshots.items[chan._snapshot_idx] };
		Tree_level_index       const  child_lvl { chan._t1_blk_idx };
		Physical_block_address const  child_pba { chan._new_pbas.pbas[child_lvl] };

		snap.pba = child_pba;
		calc_sha256_4k_hash(&chan._t1_blks.items[child_lvl], &snap.hash);

		if (VERBOSE_REKEYING)
			log("      lvl ", (Tree_level_index)snap.max_level + 1,
			    ": new snap: ", snap);

		Snapshots_index next_snap_idx { 0 };
		if (_find_next_snap_to_rekey_vba_at(chan, next_snap_idx)) {

			chan._snapshot_idx = next_snap_idx;
			Snapshot const &snap { req._snapshots.items[chan._snapshot_idx] };

			chan._first_snapshot = false;
			chan._t1_blk_idx = snap.max_level;
			if (chan._t1_blks_old_pbas.items[chan._t1_blk_idx] == snap.pba) {

				progress = true;

			} else {

				chan._t1_blks_old_pbas.items[chan._t1_blk_idx] = snap.pba;
				chan._generated_prim = {
					.op     = Generated_prim::READ,
					.succ   = false,
					.tg     = Channel::TAG_VBD_CACHE,
					.blk_nr = snap.pba,
					.idx    = chan_idx
				};
				chan._state = Channel::READ_ROOT_NODE_PENDING;
				progress = true;

				if (VERBOSE_REKEYING)
					log("    snapshot ", chan._snapshot_idx, ":");
			}

		} else {

			_mark_req_successful(chan, progress);
		}
		break;
	}
	case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_COMPLETED:

		if (_handle_failed_generated_req(chan, progress))
			break;

		chan._state = Channel::WRITE_INNER_NODE_COMPLETED;
		progress = true;

	default:

		break;
	}
}


void Virtual_block_device::_execute_vbd_extension_step(Channel &, bool &)
{
	class Program_error_vbd_extension_step { };
	throw Program_error_vbd_extension_step { };
}


void Virtual_block_device::execute(bool &progress)
{
	for (unsigned idx = 0; idx < NR_OF_CHANNELS; idx++) {

		Channel &channel = _channels[idx];
		Request &request { channel._request };

		switch (request._type) {
		case Request::INVALID:
			break;
		case Request::READ_VBA:
			_execute_read_vba(channel, idx, progress);
			break;
		case Request::WRITE_VBA:
			_execute_write_vba(channel, idx, progress);
			break;
		case Request::REKEY_VBA:
			_execute_rekey_vba(channel, idx, progress);
			break;
		case Request::VBD_EXTENSION_STEP:
			_execute_vbd_extension_step(channel, progress);
			break;
		}
	}
}


bool Virtual_block_device::_peek_generated_request(uint8_t *buf_ptr,
                                                   size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };
		Request &req { chan._request };
		if (req._type == Request::INVALID)
			continue;

		switch (chan._state) {
		case Channel::WRITE_ROOT_NODE_PENDING:
		case Channel::WRITE_INNER_NODE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::WRITE, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				&chan._t1_blks.items[chan._t1_blk_idx], nullptr);

			return true;

		case Channel::WRITE_LEAF_NODE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::WRITE, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				&chan._data_blk, nullptr);

			return true;

		case Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::WRITE_CLIENT_DATA, req._client_req_offset,
				req._client_req_tag, req._new_key_id,
				chan._generated_prim.blk_nr, chan._vba, 1, nullptr,
				&chan._hash);

			return true;

		case Channel::READ_ROOT_NODE_PENDING:
		case Channel::READ_INNER_NODE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::READ, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				&chan._t1_blks.items[chan._t1_blk_idx], nullptr);

			return true;

		case Channel::READ_LEAF_NODE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::READ, 0, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, &chan._data_blk, nullptr);

			return true;

		case Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_PENDING:

			construct_in_buf<Block_io_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::READ_CLIENT_DATA, req._client_req_offset,
				req._client_req_tag, req._new_key_id,
				chan._generated_prim.blk_nr, chan._vba, 1, nullptr, nullptr);

			return true;

		case Channel::DECRYPT_LEAF_NODE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Crypto_request::DECRYPT,
				0, 0, req._old_key_id, nullptr,
				chan._generated_prim.blk_nr, 0, &chan._data_blk,
				&chan._data_blk);

			return true;

		case Channel::ENCRYPT_LEAF_NODE_PENDING:

			construct_in_buf<Crypto_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Crypto_request::ENCRYPT,
				0, 0, req._new_key_id, nullptr,
				chan._generated_prim.blk_nr, 0, &chan._data_blk,
				&chan._data_blk);

			return true;

		case Channel::ALLOC_PBAS_AT_LEAF_LVL_PENDING:
		case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_PENDING:
		case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_PENDING:
		{
			Free_tree_request::Type ftrt { Free_tree_request::INVALID };
			switch (chan._generated_prim.tg) {
			case Channel::TAG_VBD_FT_ALLOC_FOR_NON_RKG:           ftrt = Free_tree_request::ALLOC_FOR_NON_RKG; break;
			case Channel::TAG_VBD_FT_ALLOC_FOR_RKG_CURR_GEN_BLKS: ftrt = Free_tree_request::ALLOC_FOR_RKG_CURR_GEN_BLKS; break;
			case Channel::TAG_VBD_FT_ALLOC_FOR_RKG_OLD_GEN_BLKS:  ftrt = Free_tree_request::ALLOC_FOR_RKG_OLD_GEN_BLKS; break;
			default: break;
			}
			if (ftrt == Free_tree_request::INVALID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			construct_in_buf<Free_tree_request>(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				ftrt, req._ft_root_pba_ptr,
				req._ft_root_gen_ptr, req._ft_root_hash_ptr,
				req._ft_max_level, req._ft_degree, req._ft_leaves,
				req._mt_root_pba_ptr, req._mt_root_gen_ptr,
				req._mt_root_hash_ptr, req._mt_max_level, req._mt_degree,
				req._mt_leaves, &req._snapshots, req._last_secured_generation,
				req._curr_gen, chan._free_gen, chan._nr_of_blks,
				(addr_t)&chan._new_pbas, (addr_t)&chan._t1_node_walk,
				(uint64_t)req._snapshots.items[chan._snapshot_idx].max_level,
				chan._vba, req._vbd_degree, req._vbd_highest_vba,
				req._rekeying, req._old_key_id, req._new_key_id, chan._vba);

			return true;
		}
		default: break;
		}
	}
	return false;
}


void Virtual_block_device::_drop_generated_request(Module_request &mod_req)
{
	unsigned long const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (chan._state) {
	case Channel::READ_ROOT_NODE_PENDING: chan._state = Channel::READ_ROOT_NODE_IN_PROGRESS; break;
	case Channel::READ_INNER_NODE_PENDING: chan._state = Channel::READ_INNER_NODE_IN_PROGRESS; break;
	case Channel::WRITE_ROOT_NODE_PENDING: chan._state = Channel::WRITE_ROOT_NODE_IN_PROGRESS; break;
	case Channel::WRITE_INNER_NODE_PENDING: chan._state = Channel::WRITE_INNER_NODE_IN_PROGRESS; break;
	case Channel::READ_LEAF_NODE_PENDING: chan._state = Channel::READ_LEAF_NODE_IN_PROGRESS; break;
	case Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_PENDING: chan._state = Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_IN_PROGRESS; break;
	case Channel::WRITE_LEAF_NODE_PENDING: chan._state = Channel::WRITE_LEAF_NODE_IN_PROGRESS; break;
	case Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_PENDING: chan._state = Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_IN_PROGRESS; break;
	case Channel::DECRYPT_LEAF_NODE_PENDING: chan._state = Channel::DECRYPT_LEAF_NODE_IN_PROGRESS; break;
	case Channel::ENCRYPT_LEAF_NODE_PENDING: chan._state = Channel::ENCRYPT_LEAF_NODE_IN_PROGRESS; break;
	case Channel::ALLOC_PBAS_AT_LEAF_LVL_PENDING: chan._state = Channel::ALLOC_PBAS_AT_LEAF_LVL_IN_PROGRESS; break;
	case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_PENDING: chan._state = Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_IN_PROGRESS; break;
	case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_PENDING: chan._state = Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_IN_PROGRESS; break;
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


void Virtual_block_device::generated_request_complete(Module_request &mod_req)
{
	unsigned long const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case CRYPTO:
	{
		Crypto_request &crypto_req { *static_cast<Crypto_request *>(&mod_req) };
		chan._generated_prim.succ = crypto_req.success();
		switch (chan._state) {
		case Channel::DECRYPT_LEAF_NODE_IN_PROGRESS: chan._state = Channel::DECRYPT_LEAF_NODE_COMPLETED; break;
		case Channel::ENCRYPT_LEAF_NODE_IN_PROGRESS: chan._state = Channel::ENCRYPT_LEAF_NODE_COMPLETED; break;
		default:
			class Exception_3 { };
			throw Exception_3 { };
		}
		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &blk_io_req { *static_cast<Block_io_request *>(&mod_req) };
		chan._generated_prim.succ = blk_io_req.success();
		switch (chan._state) {
		case Channel::READ_ROOT_NODE_IN_PROGRESS: chan._state = Channel::READ_ROOT_NODE_COMPLETED; break;
		case Channel::READ_INNER_NODE_IN_PROGRESS: chan._state = Channel::READ_INNER_NODE_COMPLETED; break;
		case Channel::WRITE_ROOT_NODE_IN_PROGRESS: chan._state = Channel::WRITE_ROOT_NODE_COMPLETED; break;
		case Channel::WRITE_INNER_NODE_IN_PROGRESS: chan._state = Channel::WRITE_INNER_NODE_COMPLETED; break;
		case Channel::READ_LEAF_NODE_IN_PROGRESS: chan._state = Channel::READ_LEAF_NODE_COMPLETED; break;
		case Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_IN_PROGRESS: chan._state = Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_COMPLETED; break;
		case Channel::WRITE_LEAF_NODE_IN_PROGRESS: chan._state = Channel::WRITE_LEAF_NODE_COMPLETED; break;
		case Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_IN_PROGRESS: chan._state = Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_COMPLETED; break;
		default:
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;
	}
	case FREE_TREE:
	{
		Free_tree_request &ft_req { *static_cast<Free_tree_request *>(&mod_req) };
		chan._generated_prim.succ = ft_req.success();
		switch (chan._state) {
		case Channel::ALLOC_PBAS_AT_LEAF_LVL_IN_PROGRESS: chan._state = Channel::ALLOC_PBAS_AT_LEAF_LVL_COMPLETED; break;
		case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_IN_PROGRESS: chan._state = Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_COMPLETED; break;
		case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_IN_PROGRESS: chan._state = Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_COMPLETED; break;
		default:
			class Exception_2 { };
			throw Exception_2 { };
		}
		break;
	}
	default:
		class Exception_5 { };
		throw Exception_5 { };
	}
}


bool Virtual_block_device::_peek_completed_request(uint8_t *buf_ptr,
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


void Virtual_block_device::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
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
