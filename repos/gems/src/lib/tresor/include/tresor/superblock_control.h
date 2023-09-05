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

		Type _type;
		Request_offset _client_req_offset;
		Request_tag _client_req_tag;
		Number_of_blocks _nr_of_blks;
		Virtual_block_address _vba;
		bool &_success;
		bool &_client_req_finished;
		Superblock::State &_sb_state;
		Generation &_gen;

	public:

		Superblock_control_request(Module_id, Module_request_id, Type, Request_offset,
		                           Request_tag, Number_of_blocks, Virtual_block_address,
		                           bool &, bool &, Superblock::State &, Generation &);

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		void print(Output &out) const override
		{
			Genode::print(out, type_to_string(_type));
			switch (_type) {
			case REKEY_VBA:
			case READ_VBA:
			case WRITE_VBA:
				Genode::print(out, " ", _vba);
				break;
			default:
				break;
			}
		}
};

class Tresor::Superblock_control_channel : public Module_channel
{
	friend class Superblock_control;

	private:

		using Request = Superblock_control_request;

		enum State {
			SUBMITTED, ACCESS_VBA_AT_VBD_SUCCEEDED, REKEY_VBA_AT_VBD_SUCCEEDED, CREATE_KEY_SUCCEEDED,
			READ_SB_PENDING,
			READ_SB_IN_PROGRESS,
			READ_SB_COMPLETED,
			READ_CURRENT_SB_PENDING,
			READ_CURRENT_SB_IN_PROGRESS,
			READ_CURRENT_SB_COMPLETED,
			TREE_EXT_STEP_IN_TREE_SUCCEEDED,
			ENCRYPT_CURRENT_KEY_PENDING,
			ENCRYPT_CURRENT_KEY_IN_PROGRESS,
			ENCRYPT_CURRENT_KEY_COMPLETED,
			ENCRYPT_PREVIOUS_KEY_PENDING,
			ENCRYPT_PREVIOUS_KEY_IN_PROGRESS,
			ENCRYPT_PREVIOUS_KEY_COMPLETED,
			DECRYPT_CURRENT_KEY_PENDING,
			DECRYPT_CURRENT_KEY_IN_PROGRESS,
			DECRYPT_CURRENT_KEY_COMPLETED,
			DECRYPT_PREVIOUS_KEY_PENDING,
			DECRYPT_PREVIOUS_KEY_IN_PROGRESS,
			DECRYPT_PREVIOUS_KEY_COMPLETED,
			SYNC_CACHE_PENDING,
			SYNC_CACHE_IN_PROGRESS,
			SYNC_CACHE_COMPLETED,
			ADD_KEY_AT_CRYPTO_MODULE_PENDING,
			ADD_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			ADD_KEY_AT_CRYPTO_MODULE_COMPLETED,
			ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING,
			ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED,
			ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING,
			ADD_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			ADD_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED,
			REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING,
			REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED,
			REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING,
			REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS,
			REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED,
			WRITE_SB_PENDING,
			WRITE_SB_IN_PROGRESS,
			WRITE_SB_COMPLETED,
			SYNC_BLK_IO_PENDING,
			SYNC_BLK_IO_IN_PROGRESS,
			SYNC_BLK_IO_COMPLETED,
			SECURE_SB_PENDING,
			SECURE_SB_IN_PROGRESS,
			SECURE_SB_COMPLETED,
			MAX_SB_HASH_PENDING,
			MAX_SB_HASH_IN_PROGRESS,
			MAX_SB_HASH_COMPLETED,
			COMPLETED,
			REQ_GENERATED,
		};

		enum Tag_type {
			TAG_SB_CTRL_VBD_RKG_REKEY_VBA,
			TAG_SB_CTRL_VBD_RKG_READ_VBA,
			TAG_SB_CTRL_VBD_RKG_WRITE_VBA,
			TAG_SB_CTRL_TA_ENCRYPT_KEY,
			TAG_SB_CTRL_CACHE,
			TAG_SB_CTRL_BLK_IO_READ_SB,
			TAG_SB_CTRL_BLK_IO_WRITE_SB,
			TAG_SB_CTRL_BLK_IO_SYNC,
			TAG_SB_CTRL_TA_SECURE_SB,
			TAG_SB_CTRL_TA_LAST_SB_HASH,
			TAG_SB_CTRL_TA_DECRYPT_KEY,
			TAG_SB_CTRL_CRYPTO_ADD_KEY,
			TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
		};

		struct Generated_prim
		{
			enum Type { READ, WRITE, SYNC };

			Type op { READ };
			bool succ { false };
			Tag_type tg { };
			uint64_t blk_nr { 0 };
			uint64_t idx { 0 };
		};

		State _state { SUBMITTED };
		Generated_prim _generated_prim { };
		Key _key_plaintext { };
		Superblock _sb_ciphertext { };
		Block _encoded_blk { };
		Superblock_index _sb_idx { 0 };
		bool _sb_found { false };
		Superblock_index _read_sb_idx { 0 };
		Generation _gen { INVALID_GENERATION };
		Hash _hash { };
		Key _curr_key_plaintext { };
		Key _prev_key_plaintext { };
		Physical_block_address _pba { 0 };
		Number_of_blocks _nr_of_leaves { 0 };
		Type_1_node _ft_root { };
		Tree_level_index _ft_max_lvl { 0 };
		Number_of_leaves _ft_nr_of_leaves { 0 };
		Request *_req_ptr { nullptr };

		void _generated_req_completed(State_uint) override;

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == COMPLETED; }

		void _mark_req_successful(bool &);

		void _mark_req_failed(bool &, char const *);

		void _access_vba(Superblock_control &, Virtual_block_device_request::Type, bool &);

		void _generate_vbd_req(Superblock_control &, Virtual_block_device_request::Type,
		                       State, bool &, Key_id, Virtual_block_address);

		void _generate_ta_req(Trust_anchor_request::Type, State, bool &, Key_value &);
};

class Tresor::Superblock_control : public Module
{
	friend class Superblock_control_channel;

	private:

		using Request = Superblock_control_request;
		using Channel = Superblock_control_channel;
		using Generated_prim = Channel::Generated_prim;
		using Tag = Channel::Tag_type;

		enum { NUM_CHANNELS = 1 };

		Superblock _sb { };
		Superblock_index _sb_idx { 0 };
		Generation _curr_gen { 0 };
		Channel _channels[NUM_CHANNELS] { };

		static char const *_state_to_step_label(Channel::State state);

		bool _handle_failed_generated_req(Channel &chan,
		                                  bool &progress);

		void _secure_sb_init(Channel &chan,
		                     uint64_t chan_idx,
		                     bool &progress);

		void _secure_sb_encr_curr_key_compl(Channel &chan,
		                                    uint64_t chan_idx,
		                                    bool &progress);

		void _secure_sb_encr_prev_key_compl(Channel &chan,
		                                    uint64_t chan_idx,
		                                    bool &progress);

		void _secure_sb_sync_cache_compl(Channel &chan,
		                                 uint64_t chan_idx,
		                                 bool &progress);

		void _secure_sb_write_sb_compl(Channel &chan,
		                               uint64_t chan_idx,
		                               bool &progress);

		void _secure_sb_sync_blk_io_compl(Channel &chan,
		                                  uint64_t chan_idx,
		                                  bool &progress);

		bool _secure_sb_finish(Channel &chan,
		                       bool &progress);

		void _init_sb_without_key_values(Superblock const &, Superblock &);

		void _execute_sync(Channel &, uint64_t const job_idx, Superblock &,
		                   Superblock_index &, Generation &, bool &progress);

		void _execute_create_snap(Channel &, uint64_t, bool &progress);

		void _execute_discard_snap(Channel &, uint64_t, bool &progress);

		void _execute_tree_ext_step(Channel &chan,
		                            uint64_t chan_idx,
		                            Superblock::State tree_ext_sb_state,
		                            bool tree_ext_verbose,
		                            String<4> tree_name,
		                            bool &progress);

		void _execute_rekey_vba(Channel &chan,
		                        uint64_t chan_idx,
		                        bool &progress);

		void _execute_initialize_rekeying(Channel &chan,
		                                  uint64_t chan_idx,
		                                  bool &progress);

		void _execute_initialize(Channel &, uint64_t const job_idx,
		                         Superblock &, Superblock_index &,
		                         Generation &, bool &progress);

		void _execute_deinitialize(Channel &, uint64_t const job_idx,
		                           Superblock &, Superblock_index &,
		                           Generation &, bool &progress);


		/************
		 ** Module **
		 ************/

		void execute(bool &) override;

		bool _peek_generated_request(uint8_t *, size_t) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

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
