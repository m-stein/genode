//
//		/**
//		 * Query list of active snapshots
//		 *
//		 * \param  ids  reference to destination buffer
//		 */
//		void active_snapshot_ids(Active_snapshot_ids &ids) const;
//
//		/**
//		 * Get highest virtual-block-address useable by the current active snapshot
//		 *
//		 * \return  highest addressable virtual-block-address
//		 */
//		Virtual_block_address max_vba() const;
//
//		/**
//		 * Get information about the CBE
//		 *
//		 * \return  information structure
//		 */
//		Info info() const
//		{
//			Info inf { };
//			_info(inf);
//			return inf;
//		}

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

#ifndef _SUPERBLOCK_CONTROL_H_
#define _SUPERBLOCK_CONTROL_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>

namespace Cbe
{
	class Superblock_control;
	class Superblock_control_request;
	class Superblock_control_channel;
}

class Cbe::Superblock_control_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, READ_VBA = 1, WRITE_VBA = 2, SYNC = 3, INITIALIZE = 4,
			DEINITIALIZE = 5,
			VBD_EXTENSION_STEP = 6,
			FT_EXTENSION_STEP = 7,
			CREATE_SNAPSHOT = 8,
			DISCARD_SNAPSHOT = 9,
			INITIALIZE_REKEYING = 10,
			REKEY_VBA = 11
		};

	private:

		friend class Superblock_control;
		friend class Superblock_control_channel;

		Type                  _type                    { INVALID };
		Genode::uint64_t      _client_req_offset       { 0 };
		Genode::uint64_t      _client_req_tag          { 0 };
		Virtual_block_address _vba                     { 0 };
		Genode::uint8_t       _prim[PRIM_BUF_SIZE]     { 0 };
		Superblock_state      _sb_state                { INVALID };
		bool                  _success                 { false };

	public:

		Superblock_control_request() { }

		Type type() const { return _type; }

		Superblock_control_request(unsigned long src_module_id,
		                           unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   size_t            prim_size,
		                   Genode::uint64_t  client_req_offset,
		                   Genode::uint64_t  client_req_tag,
		                   Genode::uint64_t  vba);

		void *prim_ptr() { return (void *)&_prim; }

		Superblock_state sb_state() { return _sb_state; }

		bool success() const { return _success; }


		/********************
		 ** Module_request **
		 ********************/

		char const *type_name() override;
};

class Cbe::Superblock_control_channel
{
	private:

		friend class Superblock_control;

		enum State {
			SUBMITTED,
			READ_VBA_AT_VBD_PENDING,
			READ_VBA_AT_VBD_IN_PROGRESS,
			READ_VBA_AT_VBD_COMPLETED,
			WRITE_VBA_AT_VBD_PENDING,
			WRITE_VBA_AT_VBD_IN_PROGRESS,
			WRITE_VBA_AT_VBD_COMPLETED,
			READ_SB_PENDING,
			READ_SB_IN_PROGRESS,
			READ_SB_COMPLETED,
			READ_CURRENT_SB_PENDING,
			READ_CURRENT_SB_IN_PROGRESS,
			READ_CURRENT_SB_COMPLETED,
			REKEY_VBA_IN_VBD_PENDING,
			REKEY_VBA_IN_VBD_IN_PROGRESS,
			REKEY_VBA_IN_VBD_COMPLETED,
			VBD_EXT_STEP_IN_VBD_PENDING,
			VBD_EXT_STEP_IN_VBD_IN_PROGRESS,
			VBD_EXT_STEP_IN_VBD_COMPLETED,
			FT_EXT_STEP_IN_FT_PENDING,
			FT_EXT_STEP_IN_FT_IN_PROGRESS,
			FT_EXT_STEP_IN_FT_COMPLETED,
			CREATE_KEY_PENDING,
			CREATE_KEY_IN_PROGRESS,
			CREATE_KEY_COMPLETED,
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
			LAST_SB_HASH_PENDING,
			LAST_SB_HASH_IN_PROGRESS,
			LAST_SB_HASH_COMPLETED,
			COMPLETED
		};

		enum Tag_type {
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

			Type     op     { READ };
			bool     succ   { false };
			Tag_type tg     { };
			uint64_t blk_nr { 0 };
			uint64_t idx    { 0 };
		};

		State                      _state              { SUBMITTED };
		Superblock_control_request _request            { };
		Generated_prim             _generated_prim     { };
		Key_new                    _key_plaintext      { };
		Block_data                 _sb_ciphertext_blk  { };
		Superblocks_index          _sb_idx             { 0 };
		bool                       _sb_found           { false };
		Superblocks_index          _read_sb_idx        { 0 };
		Generation                 _generation         { 0 };
		Snapshots                  _snapshots          { };
		Hash_new                   _hash               { };
		Key_new                    _curr_key_plaintext { };
		Key_new                    _prev_key_plaintext { };
		Block_data                 _blk_io_data        { };

		Superblock &_sb_ciphertext() { return *(Superblock *)&_sb_ciphertext_blk; }

	public:

		Superblock_control_request const &request() const { return _request; }
};

class Cbe::Superblock_control : public Module
{
	private:

		using Request = Superblock_control_request;
		using Channel = Superblock_control_channel;

		enum { NR_OF_CHANNELS = 1 };

		Superblock        _superblock               { };
		Superblocks_index _sb_idx                   { 0 };
		Generation        _curr_gen                 { 0 };
		Channel           _channels[NR_OF_CHANNELS] { };

		void _init_sb_without_key_values(Superblock const &, Superblock &);

		void _execute_sync(Channel &, uint64_t const job_idx, Superblock &,
                           Superblocks_index &, Generation &, bool &progress);
		void _execute_read_vba(Channel &, uint64_t const job_idx,
		                       Superblock const &, bool &progress);

		void _execute_write_vba(Channel &, uint64_t const job_idx,
                                Superblock &, Generation const &, bool &progress);

		void _discard_disposable_snapshots(Snapshots &, Generation const,
                                           Generation const);

		void _execute_initialize(Channel &, uint64_t const job_idx,
		                         Superblock &, Superblocks_index &,
		                         Generation &, bool &progress);

		void _execute_deinitialize(Channel &, uint64_t const job_idx,
		                           Superblock &, Superblocks_index &,
		                           Generation &, bool &progress);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Virtual_block_address max_vba() const;

		void active_snapshot_ids(Active_snapshot_ids &snap_ids) const
		{
			if (_superblock.valid()) {

				for (Snapshots_index idx { 0 };
				     idx < MAX_NR_OF_SNAPSHOTS_PER_SB;
				     idx++) {

					Snapshot const &snap { _superblock.snapshots.items[idx] };
					if (snap.valid && snap.keep)
						snap_ids.values[idx] = snap.gen;
					else
						snap_ids.values[idx] = 0;
				}
			} else {

				snap_ids = Active_snapshot_ids { };
			}
		}

		Info info() const
		{
			if (_superblock.valid())

				return Info {
					true, _superblock.state == REKEYING,
					_superblock.state == EXTENDING_FT,
					_superblock.state == EXTENDING_VBD };

			else

				return Info { };
		}
};

#endif /* _SUPERBLOCK_CONTROL_H_ */
