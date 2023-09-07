/*
 * \brief  Module for management of the superblocks
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__SUPERBLOCK_CONTROL_H_
#define _TRESOR__SUPERBLOCK_CONTROL_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/virtual_block_device.h>
#include <tresor/trust_anchor.h>
#include <tresor/block_io.h>

namespace Tresor {

	class Superblock_control;
	class Superblock_control_request;
	class Superblock_control_channel;
}

class Tresor::Superblock_control_request : Module_request, Noncopyable
{
	friend class Superblock_control;
	friend class Superblock_control_channel;

	public:

		enum Type {
			READ_VBA, WRITE_VBA, SYNC, INITIALIZE, DEINITIALIZE, VBD_EXTENSION_STEP,
			FT_EXTENSION_STEP, CREATE_SNAPSHOT, DISCARD_SNAPSHOT, INITIALIZE_REKEYING,
			REKEY_VBA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Number_of_blocks _nr_of_blks;
		Virtual_block_address const _vba;
		bool &_success;
		bool &_client_req_finished;
		Superblock::State &_sb_state;
		Generation &_gen;

	public:

		Superblock_control_request(Module_id, Module_request_id, Type, Request_offset,
		                           Request_tag, Number_of_blocks, Virtual_block_address,
		                           bool &, bool &, Superblock::State &, Generation &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override;
};


class Tresor::Superblock_control_channel : public Module_channel
{
	friend class Superblock_control;

	private:

		using Request = Superblock_control_request;

		enum State {
			INVALID, REQ_SUBMITTED, ACCESS_VBA_AT_VBD_SUCCEEDED,
			REKEY_VBA_AT_VBD_SUCCEEDED, CREATE_KEY_SUCCEEDED,
			ENCRYPT_CURRENT_KEY_SUCCEEDED, TREE_EXT_STEP_IN_TREE_SUCCEEDED,
			ENCRYPT_PREVIOUS_KEY_SUCCEEDED, DECRYPT_CURRENT_KEY_SUCCEEDED,
			DECRYPT_PREVIOUS_KEY_SUCCEEDED, SECURE_SB_SUCCEEDED,
			GET_LAST_SB_HASH_SUCCEEDED, ADD_KEY_AT_CRYPTO_MODULE_SUCCEEDED,
			ADD_PREV_KEY_SUCCEEDED, ADD_CURR_KEY_SUCCEEDED,
			REMOVE_PREV_KEY_SUCCEEDED, REMOVE_CURR_KEY_SUCCEEDED, READ_SB_SUCCEEDED,
			READ_CURRENT_SB_SUCCEEDED, SYNC_CACHE_SUCCEEDED, WRITE_SB_SUCCEEDED,
			SYNC_BLK_IO_SUCCEEDED, REQ_COMPLETE, REQ_GENERATED, };

		State _state { INVALID };
		Superblock _sb_ciphertext { };
		Block _encoded_blk { };
		Superblock_index _sb_idx { 0 };
		bool _sb_found { false };
		Superblock_index _read_sb_idx { 0 };
		Generation _gen { INVALID_GENERATION };
		Hash _hash { };
		Physical_block_address _pba { 0 };
		Number_of_blocks _nr_of_leaves { 0 };
		Type_1_node _ft_root { };
		Request *_req_ptr { nullptr };
		bool _gen_req_success { false };

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _access_vba(Superblock_control &, Virtual_block_device_request::Type, bool &);

		void _generate_vbd_req(Superblock_control &, Virtual_block_device_request::Type, State, bool &, Key_id, Virtual_block_address);

		void _generate_ta_req(Trust_anchor_request::Type, State, bool &, Key_value &, Key_value &);

		void _generate_blk_req(Block_io_request::Type, Physical_block_address, State, bool &);

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint complete_state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(complete_state, progress, args...);
		}
};

class Tresor::Superblock_control : public Module
{
	friend class Superblock_control_channel;

	private:

		using Request = Superblock_control_request;
		using Channel = Superblock_control_channel;

		enum { NUM_CHANNELS = 1 };

		Superblock _sb { };
		Superblock_index _sb_idx { 0 };
		Generation _curr_gen { 0 };
		Channel _channels[NUM_CHANNELS] { };

		void _secure_sb_init(Channel &, bool &);

		void _secure_sb_encr_curr_key_succ(Channel &, bool &);

		void _secure_sb_encr_prev_key_succ(Channel &, bool &);

		void _secure_sb_sync_cache_compl(Channel &, bool &);

		void _secure_sb_write_sb_compl(Channel &, bool &);

		void _secure_sb_sync_blk_io_compl(Channel &, bool &);

		void _secure_sb_finish(Channel &);

		void _init_sb_without_key_values(Superblock const &, Superblock &);

		void _execute_sync(Channel &, bool &);

		void _execute_create_snap(Channel &, bool &progress);

		void _execute_discard_snap(Channel &, bool &progress);

		void _execute_tree_ext_step(Channel &chan,
		                            Superblock::State tree_ext_sb_state,
		                            bool tree_ext_verbose,
		                            String<4> tree_name,
		                            bool &progress);

		void _execute_rekey_vba(Channel &chan,
		                        bool &progress);

		void _execute_initialize_rekeying(Channel &chan,
		                                  bool &progress);

		void _execute_initialize(Channel &,
		                         Superblock &, Superblock_index &,
		                         Generation &, bool &progress);

		void _execute_deinitialize(Channel &, bool &);


		/************
		 ** Module **
		 ************/

		void execute(bool &) override;

	public:

		Virtual_block_address max_vba() const;

		Virtual_block_address resizing_nr_of_pbas() const;

		Virtual_block_address rekeying_vba() const;

		void snapshot_generations(Snapshot_generations &generations) const
		{
			if (_sb.valid()) {

				for (Snapshot_index idx { 0 };
				     idx < MAX_NR_OF_SNAPSHOTS;
				     idx++) {

					Snapshot const &snap { _sb.snapshots.items[idx] };
					if (snap.valid && snap.keep)
						generations.items[idx] = snap.gen;
					else
						generations.items[idx] = INVALID_GENERATION;
				}
			} else {

				generations = Snapshot_generations { };
			}
		}

		Superblock_info sb_info() const
		{
			if (_sb.valid())

				return Superblock_info {
					true, _sb.state == Superblock::REKEYING,
					_sb.state == Superblock::EXTENDING_FT,
					_sb.state == Superblock::EXTENDING_VBD };

			else

				return Superblock_info { };
		}

		Superblock_control() { register_channels(_channels, NUM_CHANNELS, SUPERBLOCK_CONTROL); }
};

#endif /* _TRESOR__SUPERBLOCK_CONTROL_H_ */
