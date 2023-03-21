/*

-- Generated_Request_Complete

      when Primitive.Tag_VBD_Rkg_Crypto_Decrypt =>

         VBD_Rekeying.Mark_Generated_Prim_Completed_Plain_Data (
            Obj.VBD_Rkg, Prim, Blk_Data_Acc.all);

      when Primitive.Tag_VBD_Rkg_Crypto_Encrypt =>

         VBD_Rekeying.Mark_Generated_Prim_Completed_Cipher_Data (
            Obj.VBD_Rkg, Prim, Blk_Data_Acc.all);

      when Primitive.Tag_VBD_Rkg_Blk_IO_Read_Client_Data |
           Primitive.Tag_VBD_Rkg_Blk_IO |
           Primitive.Tag_VBD_Rkg_Cache =>

         VBD_Rekeying.Mark_Generated_Prim_Completed (
            Obj.VBD_Rkg, Prim);

      when Primitive.Tag_VBD_Rkg_Blk_IO_Write_Client_Data =>

         VBD_Rekeying.Mark_Generated_Prim_Completed_Hash (
            Obj.VBD_Rkg, Prim, Hash_Acc.All);

      when Primitive.Tag_VBD_Rkg_FT_Alloc_For_Rkg_Curr_Gen_Blks |
           Primitive.Tag_VBD_Rkg_FT_Alloc_For_Rkg_Old_Gen_Blks |
           Primitive.Tag_VBD_Rkg_FT_Alloc_For_Non_Rkg
      =>

         VBD_Rekeying.Mark_Generated_Prim_Completed_New_PBAs (
            Obj.VBD_Rkg, Prim);


-- Execute

      VBD_Rekeying.Execute (Obj.VBD_Rkg, Progress);

      Loop_Completed_Prims :
      loop
         Declare_Prim :
         declare
            Prim : constant Primitive.Object_Type :=
               VBD_Rekeying.Peek_Completed_Primitive (Obj.VBD_Rkg);
         begin
            exit Loop_Completed_Prims when not Primitive.Valid (Prim);

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA =>

               Superblock_Control.Mark_Generated_Prim_Complete (
                  Obj.SB_Ctrl, Prim);

               VBD_Rekeying.Drop_Completed_Primitive (Obj.VBD_Rkg, Prim);
               Progress := True;

            when Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA =>

               Superblock_Control.Mark_Generated_Prim_Complete_Snap (
                  Obj.SB_Ctrl,
                  Prim,
                  VBD_Rekeying.Peek_Completed_Snap (Obj.VBD_Rkg, Prim));

               VBD_Rekeying.Drop_Completed_Primitive (Obj.VBD_Rkg, Prim);
               Progress := True;

            when others =>

               raise Program_Error;

            end case;

         end Declare_Prim;
      end loop Loop_Completed_Prims;
*/



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


enum { VERBOSE_VBD = 0 };


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
	Tree_degree_log_2 const degree_log_2 = log_2(degr);
	uint64_t const degree_mask  = (1 << degree_log_2) - 1;
	return degree_mask & (vba >> (degree_log_2 * (lvl - 1)));
}


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
                                          Snapshot const        *snapshot_ptr,
                                          Tree_degree            snapshots_degree,
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
	req._snapshots.items[0]      = *snapshot_ptr;
	req._snapshots_degree        = snapshots_degree;
	req._client_req_offset       = client_req_offset;
	req._client_req_tag          = client_req_tag;
	req._curr_gen                = current_gen;
	req._new_key_id              = key_id;

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


void Virtual_block_device::_set_args_for_write_back_of_t1_lvl(Tree_level_index const max_lvl_idx,
                                                              uint64_t const  t1_lvl_idx,
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

	if (t1_lvl_idx < max_lvl_idx) {
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
			chan._request = *dynamic_cast<Request *>(&mod_req);
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

		auto const parent_lvl_idx = channel._t1_blk_idx;
		auto const child_lvl_idx  = channel._t1_blk_idx - 1;

		auto const  child_idx = child_idx_for_vba(channel._request._vba, parent_lvl_idx, channel._request._snapshots_degree);
		auto const &child     = channel._t1_blks.blk[parent_lvl_idx].nodes[child_idx];

		channel._t1_blk_idx = child_lvl_idx;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_CACHE,
			.blk_nr = child.pba,
			.idx    = job_idx
		};
		if (VERBOSE_VBD) {
			log(
				"vbd: read vba ",
				channel._vba,": lvl ", channel._t1_blk_idx, "/",
				(Tree_level_index)snapshot.max_level,
				": read inner node pba ", channel._generated_prim.blk_nr);
		}

		channel._state = Channel::State::READ_INNER_NODE_PENDING;
		progress = true;

	} else {

		Tree_level_index const parent_lvl_idx { channel._t1_blk_idx };
		Node_index const child_idx {
			child_idx_for_vba(
				channel._request._vba, parent_lvl_idx,
				channel._request._snapshots_degree) };

		Type_1_node const &child {
			channel._t1_blks.blk[parent_lvl_idx].nodes[child_idx] };

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_VBD_BLK_IO_READ_CLIENT_DATA,
			.blk_nr = child.pba,
			.idx    = job_idx
		};
		if (VERBOSE_VBD) {
			log(
				"vbd: read vba ",
				channel._vba,": lvl ", 0, "/",
				(Tree_level_index)snapshot.max_level,
				": read leaf node pba ", channel._generated_prim.blk_nr);
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
		if (VERBOSE_VBD) {
			log(
				"vbd: read vba ",
				channel._vba,": lvl ", channel._t1_blk_idx, "/",
				(Tree_level_index)snapshot.max_level,
				": read inner node pba ", channel._generated_prim.blk_nr);
		}

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
	for (unsigned lvl_idx = 0; lvl_idx <= snapshot.max_level; lvl_idx++) {
		if (lvl_idx == 0) {
			auto const  child_idx = child_idx_for_vba(vba, lvl_idx + 1, snapshot_degree);
			auto       &node      = t1_blks.blk[lvl_idx + 1].nodes[child_idx];

			node.pba   = new_pbas.pbas[lvl_idx];
			node.gen   = curr_gen;
			memcpy(node.hash, leaf_hash.bytes, HASH_SIZE);
		} else if (lvl_idx < snapshot.max_level) {
			auto const  child_idx = child_idx_for_vba(vba, lvl_idx + 1, snapshot_degree);
			auto       &node      = t1_blks.blk[lvl_idx + 1].nodes[child_idx];

			node.pba   = new_pbas.pbas[lvl_idx];
			node.gen   = curr_gen;
			calc_sha256_4k_hash(t1_blks.blk[lvl_idx].nodes, node.hash);
		} else {
			snapshot.pba   = new_pbas.pbas[lvl_idx];
			snapshot.gen   = curr_gen;
			calc_sha256_4k_hash(t1_blks.blk[lvl_idx].nodes, snapshot.hash.bytes);
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
		if (!check_sha256_4k_hash(&t1_blks.blk[t1_blk_idx], &snapshot.hash)) {
log("vbd: ", __func__, " data ", *(Block_data*)&t1_blks.blk[t1_blk_idx], " hash ", snapshot.hash);
log(__LINE__);
			class Program_error_hash_of_read_type_1 { };
			throw Program_error_hash_of_read_type_1 { };
		}
	} else {
		uint64_t    const  child_idx = child_idx_for_vba(vba, t1_blk_idx + 1, snapshots_degree);
		Type_1_node const &child     = t1_blks.blk[t1_blk_idx + 1].nodes[child_idx];
		if (!check_sha256_4k_hash(&t1_blks.blk[t1_blk_idx], &child.hash)) {
log(__LINE__);
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
		auto const &child     = t1_blks.blk[t1_blk_idx + 1].nodes[child_idx];

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
	for (unsigned lvl_idx = 0; lvl_idx <= TREE_MAX_LEVEL; lvl_idx++) {

		if (lvl_idx > snapshot.max_level) {

			new_pbas.pbas[lvl_idx] = 0;

		} else if (lvl_idx == snapshot.max_level) {

			if (snapshot.gen < curr_gen) {

				nr_of_blks++;
				new_pbas.pbas[lvl_idx] = 0;

			} else if (snapshot.gen == curr_gen) {

				new_pbas.pbas[lvl_idx] = snapshot.pba;

			} else {

				class Exception_1 { };
				throw Exception_1 { };
			}
		} else {

			Node_index const child_idx {
				child_idx_for_vba(vba, lvl_idx + 1, snapshots_degree) };

			Type_1_node const &child {
				t1_blks.blk[lvl_idx + 1].nodes[child_idx] };

			if (child.gen < curr_gen) {

				if (lvl_idx == 0 && child.gen == INVALID_GENERATION) {

					new_pbas.pbas[lvl_idx] = child.pba;

				} else {

					nr_of_blks++;
					new_pbas.pbas[lvl_idx] = 0;
				}
			} else if (child.gen == curr_gen) {

				new_pbas.pbas[lvl_idx] = child.pba;

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
	for (unsigned lvl_idx = 0; lvl_idx <= TREE_MAX_LEVEL; lvl_idx++) {
		if (lvl_idx > snapshot.max_level) {
			t1_walk.nodes[lvl_idx] = Type_1_node_unpadded { }; /* invalid */
		} else if (lvl_idx == snapshot.max_level) {
			auto &node = t1_walk.nodes[lvl_idx];

			node.pba   = snapshot.pba;
			node.gen   = snapshot.gen;
			memcpy(node.hash, snapshot.hash.bytes, HASH_SIZE);
		} else {
			auto const   child_idx = child_idx_for_vba(vba, lvl_idx + 1, snapshots_degree);
			t1_walk.nodes[lvl_idx] = *(Type_1_node_unpadded*)&t1_blks.blk[lvl_idx + 1].nodes[child_idx];
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

		if (VERBOSE_VBD) {
			log(
				"vbd: write vba ", chan._vba,": lvl ", chan._t1_blk_idx, "/",
				(Tree_level_index)chan.snapshots(chan._snapshot_idx).max_level,
				": read inner node pba ", chan._generated_prim.blk_nr);
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

			if (VERBOSE_VBD) {
				log(
					"vbd: write vba ", chan._vba,": lvl ", chan._t1_blk_idx, "/",
					(Tree_level_index)chan.snapshots(chan._snapshot_idx).max_level,
					": read inner node pba ", chan._generated_prim.blk_nr);
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

				if (VERBOSE_VBD) {
					log(
						"vbd: write vba ",
						chan._vba,": lvl ", 0, "/",
						(Tree_level_index)chan.snapshots(
							chan._snapshot_idx).max_level,
						": write leaf node pba ",
						chan._generated_prim.blk_nr);
				}
			}
		}

		break;
	case Channel::State::ALLOC_PBAS_AT_LEAF_LVL_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);

		if (VERBOSE_VBD) {
			log(
				"vbd: write vba ", chan._vba,": lvl ", chan._t1_blk_idx, "/",
				(Tree_level_index)chan.snapshots(chan._snapshot_idx).max_level,
				": alloc ", chan._nr_of_blks, " pba", chan._nr_of_blks > 1 ? "s" : "");

			for (unsigned lvl_idx = TREE_MAX_LEVEL; ; lvl_idx--) {
				if (lvl_idx <= chan.snapshots(chan._snapshot_idx).max_level) {
					Type_1_node_unpadded &node = chan._t1_node_walk.nodes[lvl_idx];
					log("  lvl ", lvl_idx, " gen ", (uint64_t)node.gen, " pba ", (uint64_t)node.pba, " -> ", (uint64_t)chan._new_pbas.pbas[lvl_idx]);
				}
				if (lvl_idx == 0)
					break;
			}
		}
		_set_args_in_order_to_write_client_data_to_leaf_node(chan._new_pbas,
		                                                     job_idx,
		                                                     chan._state,
		                                                     chan._generated_prim,
		                                                     progress);
		if (VERBOSE_VBD) {
			log(
				"vbd: write vba ",
				chan._vba,": lvl ", 0, "/",
				(Tree_level_index)chan.snapshots(
					chan._snapshot_idx).max_level,
				": write leaf node pba ",
				chan._generated_prim.blk_nr);
		}
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

		if (VERBOSE_VBD) {
			log(
				"vbd: write vba ", chan._vba,": lvl ", chan._t1_blk_idx, "/",
				(Tree_level_index)chan.snapshots(chan._snapshot_idx).max_level,
				": write inner node pba ", chan._generated_prim.blk_nr);
		}
		break;

	case Channel::State::WRITE_INNER_NODE_COMPLETED:

		_check_that_primitive_was_successful(chan._generated_prim);
		chan._t1_blk_idx = chan._t1_blk_idx + 1;

		_set_args_for_write_back_of_t1_lvl(
			chan.snapshots(chan._snapshot_idx).max_level,
			chan._t1_blk_idx, chan._new_pbas.pbas[chan._t1_blk_idx],
			job_idx, chan._state, progress, chan._generated_prim);

		if (VERBOSE_VBD) {
			log(
				"vbd: write vba ",
				chan._vba,": lvl ", chan._t1_blk_idx, "/",
				(Tree_level_index)chan.snapshots(
					chan._snapshot_idx).max_level,
				": write inner node pba ",
				chan._generated_prim.blk_nr);
		}

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


void Virtual_block_device::_execute_rekey_vba(Channel &, bool &)
{
	class Program_error_rekey_vba { };
	throw Program_error_rekey_vba { };
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
			_execute_rekey_vba(channel, progress);
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

			memcpy(
				(void *)&chan._blk_io_data,
				(void *)&chan._t1_blks.blk[chan._t1_blk_idx], BLOCK_SIZE);

			Block_io_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::WRITE, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				(void *)&chan._blk_io_data);

			return true;

		case Channel::WRITE_LEAF_NODE_PENDING:

			chan._blk_io_data = chan._data_blk;
			Block_io_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::WRITE, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				(void *)&chan._blk_io_data);

			return true;

		case Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_PENDING:

			Block_io_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::WRITE_CLIENT_DATA, req._client_req_offset,
				req._client_req_tag, nullptr, 0, req._new_key_id,
				chan._generated_prim.blk_nr, chan._vba, 1, nullptr);

			return true;

		case Channel::READ_ROOT_NODE_PENDING:
		case Channel::READ_INNER_NODE_PENDING:

			Block_io_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::READ, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				(void *)&chan._blk_io_data);

			return true;

		case Channel::READ_LEAF_NODE_PENDING:

			Block_io_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::READ, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1,
				(void *)&chan._blk_io_data);

			return true;

		case Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_PENDING:

			Block_io_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Block_io_request::READ_CLIENT_DATA, req._client_req_offset,
				req._client_req_tag, nullptr, 0, req._new_key_id,
				chan._generated_prim.blk_nr, chan._vba, 1, nullptr);

			return true;

		case Channel::DECRYPT_LEAF_NODE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Crypto_request::DECRYPT,
				0, 0, nullptr, 0, req._old_key_id, nullptr,
				chan._generated_prim.blk_nr, 0, nullptr,
				(void *)&chan._data_blk);

			return true;

		case Channel::ENCRYPT_LEAF_NODE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, VIRTUAL_BLOCK_DEVICE, id,
				Crypto_request::ENCRYPT,
				0, 0, nullptr, 0, req._new_key_id, nullptr,
				chan._generated_prim.blk_nr, 0, (void *)&chan._data_blk,
				nullptr);

			return true;

		case Channel::ALLOC_PBAS_AT_LEAF_LVL_PENDING:
		case Channel::ALLOC_PBAS_AT_HIGHER_INNER_LVL_PENDING:
		case Channel::ALLOC_PBAS_AT_LOWEST_INNER_LVL_PENDING:

			if (chan._generated_prim.tg != Channel::TAG_VBD_FT_ALLOC_FOR_NON_RKG) {
				class Exception_9 { };
				throw Exception_9 { };
			}
			Free_tree_request::create(
				buf_ptr,
				buf_size,
				VIRTUAL_BLOCK_DEVICE,
				id,
				Free_tree_request::ALLOC_FOR_NON_RKG,
				req._ft_root_pba_ptr,
				req._ft_root_gen_ptr,
				req._ft_root_hash_ptr,
				req._ft_max_level,
				req._ft_degree,
				req._ft_leaves,
				req._mt_root_pba_ptr,
				req._mt_root_gen_ptr,
				req._mt_root_hash_ptr,
				req._mt_max_level,
				req._mt_degree,
				req._mt_leaves,
				&req._snapshots,
				req._last_secured_generation,
				req._curr_gen,
				chan._free_gen,
				chan._nr_of_blks,
				(addr_t)&chan._new_pbas,
				(addr_t)&chan._t1_node_walk,
				req._snapshots.items[chan._snapshot_idx].max_level,
				nullptr,
				0,
				chan._vba,
				req._vbd_degree,
				req._vbd_highest_vba,
				req._rekeying,
				req._old_key_id,
				req._new_key_id,
				chan._vba);

			return true;

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
		Crypto_request &crypto_req { *dynamic_cast<Crypto_request *>(&mod_req) };
		memcpy(&chan._data_blk, crypto_req.result_blk_ptr(), BLOCK_SIZE);
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
		Block_io_request &blk_io_req { *dynamic_cast<Block_io_request *>(&mod_req) };
		chan._generated_prim.succ = blk_io_req.success();
		switch (chan._state) {
		case Channel::READ_ROOT_NODE_IN_PROGRESS:
			memcpy((void *)&chan._t1_blks.blk[chan._t1_blk_idx],
			       (void *)&chan._blk_io_data, BLOCK_SIZE);
			chan._state = Channel::READ_ROOT_NODE_COMPLETED;
			break;
		case Channel::READ_INNER_NODE_IN_PROGRESS:
			memcpy((void *)&chan._t1_blks.blk[chan._t1_blk_idx],
			       (void *)&chan._blk_io_data, BLOCK_SIZE);
			chan._state = Channel::READ_INNER_NODE_COMPLETED;
			break;
		case Channel::WRITE_ROOT_NODE_IN_PROGRESS:
			chan._state = Channel::WRITE_ROOT_NODE_COMPLETED;
			break;
		case Channel::WRITE_INNER_NODE_IN_PROGRESS:
			chan._state = Channel::WRITE_INNER_NODE_COMPLETED;
			break;
		case Channel::READ_LEAF_NODE_IN_PROGRESS:
			chan._data_blk = chan._blk_io_data;
			chan._state = Channel::READ_LEAF_NODE_COMPLETED;
			break;
		case Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_IN_PROGRESS:
			chan._state = Channel::READ_CLIENT_DATA_FROM_LEAF_NODE_COMPLETED;
			break;
		case Channel::WRITE_LEAF_NODE_IN_PROGRESS:
			chan._state = Channel::WRITE_LEAF_NODE_COMPLETED;
			break;
		case Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_IN_PROGRESS:
			memcpy(&chan._hash, blk_io_req.hash_ptr(), HASH_SIZE);
			chan._state = Channel::WRITE_CLIENT_DATA_TO_LEAF_NODE_COMPLETED;
			break;
		default:
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;
	}
	case FREE_TREE:
	{
		Free_tree_request &ft_req { *dynamic_cast<Free_tree_request *>(&mod_req) };
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
