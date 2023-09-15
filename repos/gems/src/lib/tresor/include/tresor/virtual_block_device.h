/*
 * \brief  Module for accessing and managing trees of the virtual block device
 * \author Martin Stein
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VIRTUAL_BLOCK_DEVICE_H_
#define _TRESOR__VIRTUAL_BLOCK_DEVICE_H_

/* base includes */
#include <util/reconstructible.h>

/* tresor includes */
#include <tresor/module.h>
#include <tresor/types.h>
#include <tresor/free_tree.h>

namespace Tresor {

	class Virtual_block_device;
	class Virtual_block_device_request;
	class Virtual_block_device_channel;
}

class Tresor::Virtual_block_device_request : public Module_request
{
	public:

		enum Type { REKEY_VBA, READ_VBA, WRITE_VBA, VBD_EXTENSION_STEP };

	private:

		friend class Virtual_block_device;
		friend class Virtual_block_device_channel;

		Type _type;
		Virtual_block_address _vba;
		Snapshots &_snapshots;
		Snapshot_index _curr_snap_idx;
		Tree_degree _snap_degr;
		Generation _curr_gen;
		Key_id _curr_key_id;
		Key_id _prev_key_id;
		Free_tree_root &_ft;
		Meta_tree_root &_mt;
		Tree_degree _vbd_degree;
		Virtual_block_address _vbd_highest_vba;
		bool _rekeying;
		Request_offset _client_req_offset;
		Request_tag _client_req_tag;
		Generation _last_secured_generation;
		Physical_block_address &_pba;
		Number_of_blocks &_nr_of_pbas;
		Number_of_leaves &_nr_of_leaves;
		bool &_success;

	public:

		Virtual_block_device_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Generation,
		                             Free_tree_root &, Meta_tree_root &, Tree_degree, Virtual_block_address, bool,
		                             Virtual_block_address, Snapshot_index, Snapshots &, Tree_degree, Key_id,
		                             Key_id, Generation, Physical_block_address &, bool &, Number_of_leaves &,
		                             Number_of_blocks &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type)); }
};

class Tresor::Virtual_block_device_channel : public Module_channel
{
	friend class Virtual_block_device;

	private:

		using Request = Virtual_block_device_request;

		enum State {
			INACTIVE, SUBMITTED, REQ_GENERATED, REQ_COMPLETE, READ_ROOT_NODE_SUCCEEDED,
			READ_INNER_NODE_SUCCEEDED, READ_LEAF_NODE_SUCCEEDED, READ_BLK_SUCCEEDED,
			WRITE_BLK_SUCCEEDED, WRITE_CLIENT_DATA_TO_LEAF_NODE_SUCCEEDED,
			DECRYPT_LEAF_NODE_SUCCEEDED, ALLOC_PBAS_AT_LEAF_LVL_SUCCEEDED,
			ALLOC_PBAS_AT_LOWEST_INNER_LVL_SUCCEEDED,
			ALLOC_PBAS_AT_HIGHER_INNER_LVL_SUCCEEDED, ENCRYPT_LEAF_NODE_SUCCEEDED,
			WRITE_LEAF_NODE_SUCCEEDED, WRITE_INNER_NODE_SUCCEEDED,
			WRITE_ROOT_NODE_SUCCEEDED };

		struct Type_1_node_blocks
		{
			Type_1_node_block items[TREE_MAX_LEVEL] { };
		};

		Request *_req_ptr { nullptr };
		State _state { INACTIVE };
		Snapshot_index _snap_idx { 0 };
		Type_1_node_blocks _t1_blks { };
		Tree_level_index _lvl { 0 };
		Virtual_block_address _vba { 0 };
		Type_1_node_walk _t1_node_walk { };
		Tree_walk_pbas _old_pbas { };
		Tree_walk_pbas _new_pbas { };
		Hash _hash { };
		Number_of_blocks _nr_of_blks { 0 };
		Generation _last_secured_gen { 0 };
		Generation _free_gen { 0 };
		Block _encoded_blk { };
		Block _data_blk { };
		bool _first_snapshot { false };
		bool _gen_req_success { false };

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			generate_req<REQUEST>(complete_state, progress, args..., _gen_req_success);
			_state = REQ_GENERATED;
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _generated_req_completed(State_uint) override;

		void _generate_ft_req(State, bool, Free_tree_request::Type);

		Free_tree_request::Type _ft_rkg_alloc_type() const;

		Snapshot &snap() { return _req_ptr->_snapshots.items[_snap_idx]; }

		void _log_rekeying_pba_alloc() const;

		void _generate_write_node_req(bool &);

		bool _find_next_snap_to_rekey_vba_at(Snapshot_index &) const;

		void _read_vba(bool &);

		void _check_and_decode_read_t1_blk(bool &);

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _set_new_pbas_and_nr_of_blks_for_alloc();

		void _generate_ft_alloc_req_for_write_vba(bool &);

		void _write_vba(bool &);

		void _update_nodes_of_branch_of_written_vba();

		void _rekey_vba(bool &);

		void _set_args_for_alloc_of_new_pbas_for_rekeying(Tree_level_index);
};

class Tresor::Virtual_block_device : public Module
{
	private:

		using Channel = Virtual_block_device_channel;
		using Request = Virtual_block_device_request;

		enum { NR_OF_CHANNELS = 1 };
		enum { MAX_T1_NODE_BLKS_IDX = 6 };

		Channel _channels[NR_OF_CHANNELS] { };

		static char const *_state_to_step_label(Channel::State state);

		bool _handle_failed_generated_req(Channel &chan,
		                                  bool    &progress);

		void _execute_vbd_extension_step (Channel &, bool &);

		void _execute_read_vba_read_inner_node_completed(Channel &channel,
		                                                 bool &progress);

		Virtual_block_address _tree_max_max_vba(Tree_degree     snap_degree,
		                                        Snapshot const &snap);

		void
		_alloc_pba_from_resizing_contingent(Physical_block_address &first_pba,
		                                    Number_of_blocks       &nr_of_pbas,
		                                    Physical_block_address &allocated_pba);

		void _set_new_pbas_identical_to_current_pbas(Channel &chan);

		void
		_add_new_branch_to_snap_using_pba_contingent(Channel           &chan,
		                                             Tree_level_index   mount_at_lvl,
		                                             Tree_node_index    mount_at_child_idx);

		void
		_set_args_for_alloc_of_new_pbas_for_resizing(Channel          &chan,
		                                             Tree_level_index  min_lvl,
		                                             bool             &progress);

		void _add_new_root_lvl_to_snap_using_pba_contingent(Channel &chan);

		void execute(bool &) override;

	public:

		Virtual_block_device() { register_channels<Channel>(_channels, NR_OF_CHANNELS, VIRTUAL_BLOCK_DEVICE); };
};

#endif /* _TRESOR__VIRTUAL_BLOCK_DEVICE_H_ */
