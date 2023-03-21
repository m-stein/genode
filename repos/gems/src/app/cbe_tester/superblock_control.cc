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

/* base includes */
#include <base/log.h>

/* cbe tester includes */
#include <superblock_control.h>
#include <crypto.h>
#include <block_io.h>
#include <trust_anchor.h>
#include <virtual_block_device.h>
#include <sha256_4k_hash.h>

using namespace Genode;
using namespace Cbe;

enum { VERBOSE_SUPERBLOCK_CONTROL = 0 };


/********************************
 ** Superblock_control_request **
 ********************************/

void Superblock_control_request::create(void     *buf_ptr,
                                        size_t    buf_size,
                                        uint64_t  src_module_id,
                                        uint64_t  src_request_id,
                                        size_t    req_type,
                                        void     *prim_ptr,
                                        size_t    prim_size,
                                        uint64_t  client_req_offset,
                                        uint64_t  client_req_tag,
                                        uint64_t  vba)
{
	Superblock_control_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;

	if (prim_size > sizeof(req._prim)) {
		class Bad_size_1 { };
		throw Bad_size_1 { };
	}
	memcpy(&req._prim, prim_ptr, prim_size);
	req._client_req_offset = client_req_offset;
	req._client_req_tag = client_req_tag;
	req._vba = vba;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Superblock_control_request::
Superblock_control_request(unsigned long src_module_id,
                           unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, SUPERBLOCK_CONTROL }
{ }


char const *Superblock_control_request::type_name()
{
	switch (_type) {
	case INVALID: return "invalid";
	case READ_VBA: return "read_vba";
	case WRITE_VBA: return "write_vba";
	case SYNC: return "sync";
	case INITIALIZE: return "initialize";
	case DEINITIALIZE: return "deinitialize";
	case VBD_EXTENSION_STEP: return "vbd_ext_step";
	case FT_EXTENSION_STEP: return "ft_ext_step";
	case CREATE_SNAPSHOT: return "create_snap";
	case DISCARD_SNAPSHOT: return "discard_snap";
	case INITIALIZE_REKEYING: return "init_rekeying";
	case REKEY_VBA: return "rekey_vba";
	}
	return "?";
}


/************************
 ** Superblock_control **
 ************************/

Virtual_block_address Superblock_control::max_vba() const
{
	if (_superblock.valid())
		return _superblock.snapshots.items[_superblock.curr_snap].nr_of_leaves - 1;
	else
		return 0;
}


void Superblock_control::_execute_read_vba(Channel          &channel,
                                           uint64_t   const job_idx,
                                           Superblock const &sb,
                                           bool             &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
		switch (sb.state) {
		case Superblock_state::REKEYING: {
			auto const vba = channel._request._vba;

			if (vba < sb.rekeying_vba)
				channel._curr_key_plaintext.id = sb.current_key.id;
			else
				channel._curr_key_plaintext.id = sb.previous_key.id;

			break;
		}
		case Superblock_state::NORMAL:
		case Superblock_state::EXTENDING_FT:
		case Superblock_state::EXTENDING_VBD:
			channel._curr_key_plaintext.id = sb.current_key.id;
			break;
		case Superblock_state::INVALID:
			class Superblock_not_valid { };
			throw Superblock_not_valid { };

			break;
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_VBD_RKG_READ_VBA,
			.blk_nr = channel._request._vba,
			.idx    = job_idx
		};

		channel._state = Channel::State::READ_VBA_AT_VBD_PENDING;
		progress = true;

		break;
	case Channel::State::READ_VBA_AT_VBD_COMPLETED:
		channel._request._success = channel._generated_prim.succ;
		channel._state = Channel::State::COMPLETED;
		progress = true;

		break;
	default:
		break;
	}
}


void Superblock_control::_execute_write_vba(Channel         &channel,
                                            uint64_t   const job_idx,
                                            Superblock       &sb,
                                            Generation const &curr_gen,
                                            bool             &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:
		switch (sb.state) {
		case Superblock_state::REKEYING: {
			auto const vba = channel._request._vba;

			if (vba < sb.rekeying_vba)
				channel._curr_key_plaintext.id = sb.current_key.id;
			else
				channel._curr_key_plaintext.id = sb.previous_key.id;

			break;
		}
		case Superblock_state::NORMAL:
		case Superblock_state::EXTENDING_FT:
		case Superblock_state::EXTENDING_VBD:
			channel._curr_key_plaintext.id = sb.current_key.id;

			break;
		case Superblock_state::INVALID:
			class Superblock_not_valid_write { };
			throw Superblock_not_valid_write { };

			break;
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_VBD_RKG_WRITE_VBA,
			.blk_nr = channel._request._vba,
			.idx    = job_idx
		};

		channel._state = Channel::State::WRITE_VBA_AT_VBD_PENDING;
		progress = true;

		break;
	case Channel::State::WRITE_VBA_AT_VBD_COMPLETED:
		if (sb.snapshots.items[sb.curr_snap].gen < curr_gen) {

			sb.curr_snap = idx_of_invalid_or_lowest_gen_evictable_snap(sb.snapshots, curr_gen, sb.last_secured_generation);

			sb.snapshots.items[sb.curr_snap] = channel._snapshots.items[0];
			sb.snapshots.items[sb.curr_snap].keep = false;
		} else if (sb.snapshots.items[sb.curr_snap].gen == curr_gen) {
			sb.snapshots.items[sb.curr_snap] = channel._snapshots.items[0];
		} else {
			class Superblock_write_vba_at_vbd { };
			throw Superblock_write_vba_at_vbd { };
		}

		channel._request._success = channel._generated_prim.succ;
		channel._state = Channel::State::COMPLETED;
		progress = true;

		break;
	default:
		break;
	}
}


void Superblock_control::_discard_disposable_snapshots(Snapshots &snapshots,
                                                       Generation const curr_gen,
                                                       Generation const last_secured_gen)
{
	for (auto &snapshot : snapshots.items)
	{
		if (snapshot.valid && !snapshot.keep &&
		    snapshot.gen != curr_gen && snapshot.gen != last_secured_gen)
			snapshot.valid = false;
	}
}


void Superblock_control::_init_sb_without_key_values(Superblock const &sb_in,
                                                     Superblock       &sb_out)
{
	sb_out.state                   = sb_in.state;
	sb_out.rekeying_vba            = sb_in.rekeying_vba;
	sb_out.resizing_nr_of_pbas     = sb_in.resizing_nr_of_pbas;
	sb_out.resizing_nr_of_leaves   = sb_in.resizing_nr_of_leaves;
	sb_out.first_pba               = sb_in.first_pba;
	sb_out.nr_of_pbas              = sb_in.nr_of_pbas;
	memset(&sb_out.previous_key.value, 0, sizeof(sb_out.previous_key.value));
	sb_out.previous_key.id         = sb_in.previous_key.id;
	memset(&sb_out.current_key.value,  0, sizeof(sb_out.current_key.value));
	sb_out.current_key.id          = sb_in.current_key.id;
	sb_out.snapshots               = sb_in.snapshots;
	sb_out.last_secured_generation = sb_in.last_secured_generation;
	sb_out.curr_snap               = sb_in.curr_snap;
	sb_out.degree                  = sb_in.degree;
	sb_out.free_gen                = sb_in.free_gen;
	sb_out.free_number             = sb_in.free_number;
	sb_out.free_hash               = sb_in.free_hash;
	sb_out.free_max_level          = sb_in.free_max_level;
	sb_out.free_degree             = sb_in.free_degree;
	sb_out.free_leaves             = sb_in.free_leaves;
	sb_out.meta_gen                = sb_in.meta_gen;
	sb_out.meta_number             = sb_in.meta_number;
	sb_out.meta_hash               = sb_in.meta_hash;
	sb_out.meta_max_level          = sb_in.meta_max_level;
	sb_out.meta_degree             = sb_in.meta_degree;
	sb_out.meta_leaves             = sb_in.meta_leaves;
}


void Superblock_control::_execute_sync(Channel           &channel,
                                       uint64_t   const   job_idx,
                                       Superblock        &sb,
                                       Superblocks_index &sb_idx,
                                       Generation        &curr_gen,
                                       bool              &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		_discard_disposable_snapshots(
			sb.snapshots, sb.last_secured_generation, curr_gen);

		sb.last_secured_generation = curr_gen;
		sb.snapshots.items[sb.curr_snap].gen = curr_gen;
		_init_sb_without_key_values(sb, channel._sb_ciphertext());

		channel._key_plaintext = sb.current_key;
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::ENCRYPT_CURRENT_KEY_PENDING;
		progress = true;
		break;

	case Channel::State::ENCRYPT_CURRENT_KEY_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Encrypt_current_key_error { };
			throw Encrypt_current_key_error { };
		}
		switch (sb.state) {
		case Superblock_state::REKEYING:

			channel._key_plaintext = sb.previous_key;
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};
			channel._state = Channel::State::ENCRYPT_PREVIOUS_KEY_PENDING;
			progress = true;
			break;

		default:

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::SYNC,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
				.blk_nr = 0,
				.idx    = job_idx
			};
			channel._state = Channel::State::SYNC_CACHE_PENDING;
			progress = true;
			break;
		}
		break;

	case Channel::State::ENCRYPT_PREVIOUS_KEY_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Encrypt_previous_key_error { };
			throw Encrypt_previous_key_error { };
		}
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::SYNC_CACHE_PENDING;
		progress = true;
		break;

	case Channel::State::SYNC_CACHE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Sync_cache_error { };
			throw Sync_cache_error { };
		}
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_WRITE_SB,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};
		channel._state = Channel::State::WRITE_SB_PENDING;
		progress = true;
		break;

	case Channel::State::WRITE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Write_sb_completed_error { };
			throw Write_sb_completed_error { };
		}
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_SYNC,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};
		channel._state = Channel::State::SYNC_BLK_IO_PENDING;
		progress = true;
		break;

	case Channel::State::SYNC_BLK_IO_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Sync_blk_io_completed_error { };
			throw Sync_blk_io_completed_error { };
		}
		calc_sha256_4k_hash(&channel._sb_ciphertext_blk, &channel._hash);
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_SECURE_SB,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::SECURE_SB_PENDING;

		if (sb_idx < MAX_SUPERBLOCK_INDEX)
            sb_idx = sb_idx + 1;
		else
			sb_idx = 0;

		channel._generation = curr_gen;
		curr_gen = curr_gen + 1;
		progress = true;
		break;

	case Channel::State::SECURE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Secure_sb_completed_error { };
			throw Secure_sb_completed_error { };
		}
		sb.last_secured_generation = channel._generation;
		channel._request._success = true;
		channel._state = Channel::State::COMPLETED;
		progress = true;
		break;

	default:

		break;
	}
}


void Superblock_control::_execute_initialize(Channel           &channel,
                                             uint64_t const     job_idx,
                                             Superblock        &sb,
                                             Superblocks_index &sb_idx,
                                             Generation        &curr_gen,
                                             bool              &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		channel._sb_found = false;
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_LAST_SB_HASH,
			.blk_nr = 0,
			.idx    = job_idx
		};
		channel._state = Channel::State::LAST_SB_HASH_PENDING;
		progress = true;
		break;

	case Channel::State::LAST_SB_HASH_COMPLETED:

		channel._read_sb_idx = 0;
		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_READ_SB,
			.blk_nr = channel._read_sb_idx,
			.idx    = job_idx
		};
		channel._state = Channel::State::READ_SB_PENDING;
		progress = true;
		break;

	case Channel::State::READ_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Execute_initialize_error { };
			throw Execute_initialize_error { };
		}
		if (channel._sb_ciphertext().state != Superblock_state::INVALID) {

			Superblock const &cipher { channel._sb_ciphertext() };
			Snapshots_index const snap_index { newest_snapshot_idx(cipher.snapshots) };
			Generation const sb_generation { cipher.snapshots.items[snap_index].gen };

			if (check_sha256_4k_hash(&channel._sb_ciphertext_blk, channel._hash.bytes)) {
				channel._generation = sb_generation;
				channel._sb_idx     = channel._read_sb_idx;
				channel._sb_found   = true;
			}
		}
		if (channel._read_sb_idx < MAX_SUPERBLOCK_INDEX) {

			channel._read_sb_idx = channel._read_sb_idx + 1;
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_READ_SB,
				.blk_nr = channel._read_sb_idx,
				.idx    = job_idx
			};
			channel._state = Channel::State::READ_SB_PENDING;
			progress       = true;

		} else {

			if (!channel._sb_found) {
				class Execute_initialize_sb_found_error { };
				throw Execute_initialize_sb_found_error { };
			}

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_READ_SB,
				.blk_nr = channel._sb_idx,
				.idx    = job_idx
			};

			channel._state = Channel::State::READ_CURRENT_SB_PENDING;
			progress       = true;
		}
		break;

	case Channel::State::READ_CURRENT_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Execute_initialize_read_current_sb_error { };
			throw Execute_initialize_read_current_sb_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_DECRYPT_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::DECRYPT_CURRENT_KEY_PENDING;
		progress       = true;

		break;
	case Channel::State::DECRYPT_CURRENT_KEY_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Execute_initialize_decrypt_current_key_error { };
			throw Execute_initialize_decrypt_current_key_error { };
		}

		channel._curr_key_plaintext.id = channel._sb_ciphertext().current_key.id;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_ADD_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING;
		progress       = true;

		break;
	case Channel::State::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Execute_add_current_key_at_crypto_error { };
			throw Execute_add_current_key_at_crypto_error { };
		}

		switch (channel._sb_ciphertext().state) {
		case Superblock_state::INVALID:
			class Execute_add_current_key_at_crypto_invalid_error { };
			throw Execute_add_current_key_at_crypto_invalid_error { };

			break;
		case Superblock_state::REKEYING:

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_DECRYPT_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::DECRYPT_PREVIOUS_KEY_PENDING;
			progress       = true;

			break;
		case Superblock_state::NORMAL:
		case Superblock_state::EXTENDING_VBD:
		case Superblock_state::EXTENDING_FT:

			_init_sb_without_key_values(channel._sb_ciphertext(), sb);

			sb.current_key.value = channel._curr_key_plaintext.value;
			sb_idx               = channel._sb_idx;
			curr_gen             = channel._generation + 1;

			sb_idx = channel._sb_idx;
			curr_gen = channel._generation + 1;

			if (sb.free_max_level < FREE_TREE_MIN_MAX_LEVEL) {
				class Execute_add_current_key_at_crypto_max_level_error { };
				throw Execute_add_current_key_at_crypto_max_level_error { };
			}

			channel._request._sb_state = _superblock.state;
			channel._request._success = true;

			channel._state = Channel::State::COMPLETED;
			progress       = true;

			break;
		}

		break;
	case Channel::State::DECRYPT_PREVIOUS_KEY_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Decrypt_previous_key_error { };
			throw Decrypt_previous_key_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_ADD_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING;
		progress       = true;

		break;
	case Channel::State::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Add_previous_key_at_crypto_module_error { };
			throw Add_previous_key_at_crypto_module_error { };
		}

		_init_sb_without_key_values(channel._sb_ciphertext(), sb);

		sb.current_key.value  = channel._curr_key_plaintext.value;
		sb.previous_key.value = channel._prev_key_plaintext.value;

		sb_idx   = channel._sb_idx;
		curr_gen = channel._generation + 1;

		channel._request._sb_state = _superblock.state;
		channel._request._success = true;

		channel._state = Channel::State::COMPLETED;
		progress       = true;

		break;
	default:
		break;
	}
}


void Superblock_control::_execute_deinitialize(Channel           &channel,
                                               uint64_t const     job_idx,
                                               Superblock        &sb,
                                               Superblocks_index &sb_idx,
                                               Generation        &curr_gen,
                                               bool              &progress)
{
	switch (channel._state) {
	case Channel::State::SUBMITTED:

		_discard_disposable_snapshots(sb.snapshots, sb.last_secured_generation,
		                              curr_gen);

		sb.last_secured_generation           = curr_gen;
		sb.snapshots.items[sb.curr_snap].gen = curr_gen;

		_init_sb_without_key_values(sb, channel._sb_ciphertext());
		channel._key_plaintext = sb.current_key;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::ENCRYPT_CURRENT_KEY_PENDING;
		progress       = true;

		break;
	case Channel::State::ENCRYPT_CURRENT_KEY_COMPLETED:
		if (!channel._generated_prim.succ) {
			class Deinitialize_encrypt_current_key_error { };
			throw Deinitialize_encrypt_current_key_error { };
		}

		switch (sb.state) {
		case Superblock_state::REKEYING:
			channel._key_plaintext = sb.previous_key;

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_ENCRYPT_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::ENCRYPT_PREVIOUS_KEY_PENDING;
			progress       = true;

			break;

		default:
			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::SYNC,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::SYNC_CACHE_PENDING;
			progress       = true;

			break;
		}

		break;
	case Channel::State::ENCRYPT_PREVIOUS_KEY_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_encrypt_previous_key_error { };
			throw Deinitialize_encrypt_previous_key_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CACHE,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::SYNC_CACHE_PENDING;
		progress       = true;

		break;
	case Channel::State::SYNC_CACHE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_sync_cache_error { };
			throw Deinitialize_sync_cache_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::WRITE,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_WRITE_SB,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};

		channel._state = Channel::State::WRITE_SB_PENDING;
		progress       = true;

		break;
	case Channel::State::WRITE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_write_sb_error { };
			throw Deinitialize_write_sb_error { };
		}

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::SYNC,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_BLK_IO_SYNC,
			.blk_nr = sb_idx,
			.idx    = job_idx
		};

		channel._state = Channel::State::SYNC_BLK_IO_PENDING;
		progress       = true;

		break;
	case Channel::State::SYNC_BLK_IO_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_sync_blk_io_error { };
			throw Deinitialize_sync_blk_io_error { };
		}
		calc_sha256_4k_hash(&channel._sb_ciphertext_blk, &channel._hash);

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_TA_SECURE_SB,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::SECURE_SB_PENDING;

		if (sb_idx < MAX_SUPERBLOCK_INDEX)
			sb_idx = sb_idx + 1;
		else
			sb_idx = 0;

		channel._generation = curr_gen;
		curr_gen = curr_gen + 1;

		progress = true;

		break;
	case Channel::State::SECURE_SB_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_secure_sb_error { };
			throw Deinitialize_secure_sb_error { };
		}

		sb.last_secured_generation = channel._generation;

		channel._request._success = true;

		channel._curr_key_plaintext.id = sb.current_key.id;

		channel._generated_prim = {
			.op     = Channel::Generated_prim::Type::READ,
			.succ   = false,
			.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
			.blk_nr = 0,
			.idx    = job_idx
		};

		channel._state = Channel::State::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING;
		progress       = true;

		break;
	case Channel::State::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_remove_current_key_error { };
			throw Deinitialize_remove_current_key_error { };
		}

		switch (sb.state) {
		default:
			class Deinitialize_remove_current_key_invalid_error { };
			throw Deinitialize_remove_current_key_invalid_error { };
			break;
		case Superblock_state::REKEYING:

			channel._prev_key_plaintext.id = sb.previous_key.id;

			channel._generated_prim = {
				.op     = Channel::Generated_prim::Type::READ,
				.succ   = false,
				.tg     = Channel::Tag_type::TAG_SB_CTRL_CRYPTO_REMOVE_KEY,
				.blk_nr = 0,
				.idx    = job_idx
			};

			channel._state = Channel::State::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING;
			progress       = true;

			break;
		case Superblock_state::NORMAL:
		case Superblock_state::EXTENDING_VBD:
		case Superblock_state::EXTENDING_FT:

			channel._request._success = true;

			channel._state = Channel::State::COMPLETED;
			progress       = true;

			break;
		}

		break;
	case Channel::State::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED:

		if (!channel._generated_prim.succ) {
			class Deinitialize_remove_previous_key_error { };
			throw Deinitialize_remove_previous_key_error { };
		}

		sb.state = Superblock_state::INVALID;

		channel._request._success = true;

		channel._state = Channel::State::COMPLETED;
		progress       = true;

		break;
	default:
		break;
	}
}


bool Superblock_control::_peek_generated_request(uint8_t *buf_ptr,
                                                 size_t   buf_size)
{
	for (unsigned id = 0; id < NR_OF_CHANNELS; id++) {

		Channel &chan { _channels[id] };
		Request &req { chan._request };
		if (req._type == Request::INVALID)
			continue;

		switch (chan._state) {
		case Channel::CREATE_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::CREATE_KEY, nullptr, 0, nullptr,
				nullptr, nullptr, nullptr);

			return 1;

		case Channel::ENCRYPT_CURRENT_KEY_PENDING:
		case Channel::ENCRYPT_PREVIOUS_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::ENCRYPT_KEY, nullptr, 0,
				&chan._key_plaintext.value, nullptr, nullptr, nullptr);

			return 1;

		case Channel::DECRYPT_CURRENT_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::DECRYPT_KEY, nullptr, 0,
				nullptr, &chan._sb_ciphertext().current_key.value,
				nullptr, nullptr);

			return 1;

		case Channel::DECRYPT_PREVIOUS_KEY_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::DECRYPT_KEY, nullptr, 0,
				nullptr, &chan._sb_ciphertext().previous_key.value,
				nullptr, nullptr);

			return 1;

		case Channel::SECURE_SB_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::SECURE_SUPERBLOCK, nullptr, 0,
				nullptr, nullptr, nullptr, &chan._hash);

			return 1;

		case Channel::LAST_SB_HASH_PENDING:

			Trust_anchor_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Trust_anchor_request::GET_LAST_SB_HASH, nullptr, 0,
				nullptr, nullptr, nullptr, nullptr);

			return 1;

		case Channel::ADD_KEY_AT_CRYPTO_MODULE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::ADD_KEY, 0, 0, nullptr, 0,
				chan._key_plaintext.id, &chan._key_plaintext.value,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::ADD_KEY, 0, 0, nullptr, 0,
				chan._curr_key_plaintext.id, &chan._curr_key_plaintext.value,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::ADD_KEY, 0, 0, nullptr, 0,
				chan._prev_key_plaintext.id, &chan._prev_key_plaintext.value,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::REMOVE_KEY, 0, 0, nullptr, 0,
				chan._prev_key_plaintext.id, &chan._prev_key_plaintext.value,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING:

			Crypto_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Crypto_request::REMOVE_KEY, 0, 0, nullptr, 0,
				chan._curr_key_plaintext.id, &chan._curr_key_plaintext.value,
				0, 0, nullptr, nullptr);

			return 1;

		case Channel::READ_VBA_AT_VBD_PENDING:

			Virtual_block_device_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Virtual_block_device_request::READ_VBA, nullptr, 0,
				req._client_req_offset, req._client_req_tag,
				_superblock.last_secured_generation,
				(addr_t)&_superblock.free_number,
				(addr_t)&_superblock.free_gen,
				(addr_t)&_superblock.free_hash,
				_superblock.free_max_level,
				_superblock.free_degree,
				_superblock.free_leaves,
				(addr_t)&_superblock.meta_number,
				(addr_t)&_superblock.meta_gen,
				(addr_t)&_superblock.meta_hash,
				_superblock.meta_max_level,
				_superblock.meta_degree,
				_superblock.meta_leaves,
				_superblock.degree,
				max_vba(),
				_superblock.state == REKEYING ? 1 : 0,
				req._vba,
				&_superblock.snapshots.items[_superblock.curr_snap],
				_superblock.degree,
				_curr_gen,
				chan._curr_key_plaintext.id);

			return 1;

		case Channel::WRITE_VBA_AT_VBD_PENDING:

			Virtual_block_device_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Virtual_block_device_request::WRITE_VBA, nullptr, 0,
				req._client_req_offset, req._client_req_tag,
				_superblock.last_secured_generation,
				(addr_t)&_superblock.free_number,
				(addr_t)&_superblock.free_gen,
				(addr_t)&_superblock.free_hash,
				_superblock.free_max_level,
				_superblock.free_degree,
				_superblock.free_leaves,
				(addr_t)&_superblock.meta_number,
				(addr_t)&_superblock.meta_gen,
				(addr_t)&_superblock.meta_hash,
				_superblock.meta_max_level,
				_superblock.meta_degree,
				_superblock.meta_leaves,
				_superblock.degree,
				max_vba(),
				_superblock.state == REKEYING ? 1 : 0,
				req._vba,
				&_superblock.snapshots.items[_superblock.curr_snap],
				_superblock.degree,
				_curr_gen,
				chan._curr_key_plaintext.id);

			return 1;

		case Channel::READ_SB_PENDING:
		case Channel::READ_CURRENT_SB_PENDING:

			Block_io_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Block_io_request::READ, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, &chan._blk_io_data);

			return true;

		case Channel::SYNC_BLK_IO_PENDING:
		case Channel::SYNC_CACHE_PENDING:

			Block_io_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Block_io_request::SYNC, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, nullptr);

			return true;

		case Channel::WRITE_SB_PENDING:

			memcpy(&chan._blk_io_data, &chan._sb_ciphertext_blk, BLOCK_SIZE);
			Block_io_request::create(
				buf_ptr, buf_size, SUPERBLOCK_CONTROL, id,
				Block_io_request::WRITE, 0, 0, nullptr, 0, 0,
				chan._generated_prim.blk_nr, 0, 1, &chan._blk_io_data);

			return true;

		case Channel::REKEY_VBA_IN_VBD_PENDING:
		case Channel::VBD_EXT_STEP_IN_VBD_PENDING:
		case Channel::FT_EXT_STEP_IN_FT_PENDING:

			class Exception_1 { };
			throw Exception_1 { };

		default: break;
		}
	}
	return false;
}


void Superblock_control::_drop_generated_request(Module_request &mod_req)
{
	unsigned long const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	Channel &chan { _channels[id] };
	Request &req { chan._request };
	if (req._type == Request::INVALID) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	switch (chan._state) {
	case Channel::CREATE_KEY_PENDING: chan._state = Channel::CREATE_KEY_IN_PROGRESS; break;
	case Channel::ENCRYPT_CURRENT_KEY_PENDING: chan._state = Channel::ENCRYPT_CURRENT_KEY_IN_PROGRESS; break;
	case Channel::ENCRYPT_PREVIOUS_KEY_PENDING: chan._state = Channel::ENCRYPT_PREVIOUS_KEY_IN_PROGRESS; break;
	case Channel::DECRYPT_CURRENT_KEY_PENDING: chan._state = Channel::DECRYPT_CURRENT_KEY_IN_PROGRESS; break;
	case Channel::DECRYPT_PREVIOUS_KEY_PENDING: chan._state = Channel::DECRYPT_PREVIOUS_KEY_IN_PROGRESS; break;
	case Channel::SECURE_SB_PENDING: chan._state = Channel::SECURE_SB_IN_PROGRESS; break;
	case Channel::LAST_SB_HASH_PENDING: chan._state = Channel::LAST_SB_HASH_IN_PROGRESS; break;
	case Channel::ADD_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::ADD_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_PENDING: chan._state = Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS; break;
	case Channel::READ_VBA_AT_VBD_PENDING: chan._state = Channel::READ_VBA_AT_VBD_IN_PROGRESS; break;
	case Channel::WRITE_VBA_AT_VBD_PENDING: chan._state = Channel::WRITE_VBA_AT_VBD_IN_PROGRESS; break;
	case Channel::READ_SB_PENDING: chan._state = Channel::READ_SB_IN_PROGRESS; break;
	case Channel::READ_CURRENT_SB_PENDING: chan._state = Channel::READ_CURRENT_SB_IN_PROGRESS; break;
	case Channel::SYNC_BLK_IO_PENDING: chan._state = Channel::SYNC_BLK_IO_IN_PROGRESS; break;
	case Channel::SYNC_CACHE_PENDING: chan._state = Channel::SYNC_CACHE_IN_PROGRESS; break;
	case Channel::WRITE_SB_PENDING: chan._state = Channel::WRITE_SB_IN_PROGRESS; break;
	case Channel::REKEY_VBA_IN_VBD_PENDING: chan._state = Channel::REKEY_VBA_IN_VBD_IN_PROGRESS; break;
	case Channel::VBD_EXT_STEP_IN_VBD_PENDING: chan._state = Channel::VBD_EXT_STEP_IN_VBD_IN_PROGRESS; break;
	case Channel::FT_EXT_STEP_IN_FT_PENDING: chan._state = Channel::FT_EXT_STEP_IN_FT_IN_PROGRESS; break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Superblock_control::execute(bool &progress)
{
	for (unsigned idx = 0; idx < NR_OF_CHANNELS; idx++) {

		Channel &channel = _channels[idx];
		Request &request { channel._request };

		switch (request._type) {
		case Request::READ_VBA:
			_execute_read_vba(channel, idx, _superblock, progress);

			break;
		case Request::WRITE_VBA:
			_execute_write_vba(channel, idx, _superblock, _curr_gen, progress);

			break;
		case Request::SYNC:
			_execute_sync(channel, idx, _superblock, _sb_idx, _curr_gen, progress);

			break;
		case Request::INITIALIZE_REKEYING:
			class Superblock_control_initialize_rekeying { };
			throw Superblock_control_initialize_rekeying { };

			break;
		case Request::REKEY_VBA:
			class Superblock_control_rekey_vba { };
			throw Superblock_control_rekey_vba { };

			break;
		case Request::VBD_EXTENSION_STEP:
			class Superblock_control_vbd_extension_step { };
			throw Superblock_control_vbd_extension_step { };

			break;
		case Request::FT_EXTENSION_STEP:
			class Superblock_control_ft_extension_step { };
			throw Superblock_control_ft_extension_step { };

			break;
		case Request::CREATE_SNAPSHOT:
			class Superblock_control_create_snapshot { };
			throw Superblock_control_create_snapshot { };

			break;
		case Request::DISCARD_SNAPSHOT:
			class Superblock_control_discard_snapshot { };
			throw Superblock_control_discard_snapshot { };

			break;
		case Request::INITIALIZE:
			_execute_initialize(channel, idx, _superblock, _sb_idx, _curr_gen,
			                    progress);

			break;
		case Request::DEINITIALIZE:
			_execute_deinitialize (channel, idx, _superblock, _sb_idx,
			                       _curr_gen, progress);

			break;
		case Request::INVALID:
			break;
		}
	}
}


void Superblock_control::generated_request_complete(Module_request &mod_req)
{
	unsigned long const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	Channel &chan { _channels[id] };
	switch (mod_req.dst_module_id()) {
	case TRUST_ANCHOR:
	{
		Trust_anchor_request &gen_req { *dynamic_cast<Trust_anchor_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::CREATE_KEY_IN_PROGRESS:
			chan._state = Channel::CREATE_KEY_COMPLETED;
			memcpy(&chan._key_plaintext.value, gen_req.key_plaintext_ptr(), KEY_SIZE);
			break;
		case Channel::ENCRYPT_CURRENT_KEY_IN_PROGRESS:
			chan._state = Channel::ENCRYPT_CURRENT_KEY_COMPLETED;
			memcpy(&chan._sb_ciphertext().current_key.value, gen_req.key_ciphertext_ptr(), KEY_SIZE);
			break;
		case Channel::ENCRYPT_PREVIOUS_KEY_IN_PROGRESS:
			chan._state = Channel::ENCRYPT_PREVIOUS_KEY_COMPLETED;
			memcpy(&chan._sb_ciphertext().previous_key.value, gen_req.key_ciphertext_ptr(), KEY_SIZE);
			break;
		case Channel::DECRYPT_CURRENT_KEY_IN_PROGRESS:
			chan._state = Channel::DECRYPT_CURRENT_KEY_COMPLETED;
			memcpy(&chan._curr_key_plaintext.value, gen_req.key_plaintext_ptr(), KEY_SIZE);
			break;
		case Channel::DECRYPT_PREVIOUS_KEY_IN_PROGRESS:
			chan._state = Channel::DECRYPT_PREVIOUS_KEY_COMPLETED;
			memcpy(&chan._prev_key_plaintext.value, gen_req.key_plaintext_ptr(), KEY_SIZE);
			break;
		case Channel::SECURE_SB_IN_PROGRESS: chan._state = Channel::SECURE_SB_COMPLETED; break;
		case Channel::LAST_SB_HASH_IN_PROGRESS:
			chan._state = Channel::LAST_SB_HASH_COMPLETED;
			memcpy(&chan._hash, gen_req.hash_ptr(), HASH_SIZE);
			break;
		default:
			class Exception_4 { };
			throw Exception_4 { };
		}
		break;
	}
	case CRYPTO:
	{
		Crypto_request &gen_req { *dynamic_cast<Crypto_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::ADD_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::ADD_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::ADD_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::ADD_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::REMOVE_PREVIOUS_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		case Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_IN_PROGRESS: chan._state = Channel::REMOVE_CURRENT_KEY_AT_CRYPTO_MODULE_COMPLETED; break;
		default:
			class Exception_5 { };
			throw Exception_5 { };
		}
		break;
	}
	case VIRTUAL_BLOCK_DEVICE:
	{
		Virtual_block_device_request &gen_req { *dynamic_cast<Virtual_block_device_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::READ_VBA_AT_VBD_IN_PROGRESS: chan._state = Channel::READ_VBA_AT_VBD_COMPLETED; break;
		case Channel::WRITE_VBA_AT_VBD_IN_PROGRESS:
			chan._state = Channel::WRITE_VBA_AT_VBD_COMPLETED;
			chan._snapshots.items[0] = *(gen_req.snapshot_ptr());
			break;
		default:
			class Exception_6 { };
			throw Exception_6 { };
		}
		break;
	}
	case BLOCK_IO:
	{
		Block_io_request &gen_req { *dynamic_cast<Block_io_request*>(&mod_req) };
		chan._generated_prim.succ = gen_req.success();
		switch (chan._state) {
		case Channel::READ_SB_IN_PROGRESS:
			chan._state = Channel::READ_SB_COMPLETED;
			memcpy(&chan._sb_ciphertext_blk, &chan._blk_io_data, BLOCK_SIZE);
			break;
		case Channel::READ_CURRENT_SB_IN_PROGRESS:
			chan._state = Channel::READ_CURRENT_SB_COMPLETED;
			memcpy(&chan._sb_ciphertext_blk, &chan._blk_io_data, BLOCK_SIZE);
			break;
		case Channel::SYNC_BLK_IO_IN_PROGRESS: chan._state = Channel::SYNC_BLK_IO_COMPLETED; break;
		case Channel::SYNC_CACHE_IN_PROGRESS: chan._state = Channel::SYNC_CACHE_COMPLETED; break;
		case Channel::WRITE_SB_IN_PROGRESS: chan._state = Channel::WRITE_SB_COMPLETED; break;
		default:
			class Exception_7 { };
			throw Exception_7 { };
		}
		break;
	}
	default:
		class Exception_8 { };
		throw Exception_8 { };
	}
}


bool Superblock_control::_peek_completed_request(uint8_t *buf_ptr,
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


void Superblock_control::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._request._type == Request::INVALID) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	if (_channels[id]._state != Channel::COMPLETED) {
		class Exception_3 { };
		throw Exception_3 { };
	}
	_channels[id]._request._type = Request::INVALID;
}


bool Superblock_control::ready_to_submit_request()
{
	for (Channel const &channel : _channels) {
		if (channel._request._type == Request::INVALID)
			return true;
	}
	return false;
}


void Superblock_control::submit_request(Module_request &req)
{
	for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._request._type == Request::INVALID) {
			req.dst_request_id(id);
			_channels[id]._request = *dynamic_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}



/* utilities


   --
   --  Max_VBA
   --
   function Max_VBA (Obj : Object_Type)
   return Virtual_Block_Address_Type
   is (
      Superblock_Control.Max_VBA (Obj.Superblock));

   --
   --  Info
   --
   procedure Info (
      Obj  :     Object_Type;
      Info : out Info_Type)
   is
   begin
      Superblock_Control.Info (Obj.Superblock, Info);
   end Info;


   --
   --  Active_Snapshot_IDs
   --
   procedure Active_Snapshot_IDs (
      Obj  :     Object_Type;
      List : out Active_Snapshot_IDs_Type)
   is
   begin
      Superblock_Control.Active_Snapshot_IDs (Obj.Superblock, List);
   end Active_Snapshot_IDs;



   --
   --  Idx_Of_Any_Invalid_Snap
   --
   function Idx_Of_Any_Invalid_Snap (Snapshots : Snapshots_Type)
   return Snapshots_Index_Type
   is
   begin
      Find_Invalid_Snap_Idx :
      for Idx in Snapshots'Range loop
         if !Snapshots (Idx).Valid then
            return Idx;
         end if;
      end loop Find_Invalid_Snap_Idx;
      raise Program_Error;
   end Idx_Of_Any_Invalid_Snap;


      Trans_Data        : Translation_Data_Type;
      Cur_SB            : Superblocks_Index_Type;
      Cur_Gen           : Generation_Type;
      Superblock        : Superblock_Type;

      Obj.Trans_Data       := (others => (others => 0));
      Obj.Superblock := Superblock_Invalid;
      Obj.Cur_Gen := Generation_Type'First;
      Obj.Cur_SB := Superblocks_Index_Type'First;
*/


/* execute


      Superblock_Control.Execute (
         Obj.SB_Ctrl, Obj.Superblock, Obj.Cur_SB, Obj.Cur_Gen, Progress);
*/


/* Peek generated request

      Loop_Generated_FT_Rszg_Prims :
      loop
         Declare_FT_Rszg_Prim :
         declare
            Prim : constant Primitive.Object_Type :=
               Superblock_Control.Peek_Generated_FT_Rszg_Primitive (
                  Obj.SB_Ctrl);
         begin
            exit Loop_Generated_FT_Rszg_Prims when
               !Primitive.Valid (Prim);

            raise Program_Error;

         end Declare_FT_Rszg_Prim;
      end loop Loop_Generated_FT_Rszg_Prims;


      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_VBD_Rkg_Primitive (
               Obj.SB_Ctrl);
         Req_Type : CXX_Object_Size_Type := CXX_Object_Size_Type'Last;
         Block_Data : Block_Data_Type;
      begin
         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA => Req_Type := 1;
            when Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA => Req_Type := 2;
            when others => raise Program_Error;
            end case;
            Block_Data_From_Snapshot (Block_Data, 0, Superblock_Control.Peek_Generated_Snapshot (
                                                        Obj.SB_Ctrl, Prim, Obj.Superblock));
            Create_VBD_Req (
               Buf_Ptr          => Buf_Ptr,
               Buf_Size         => Buf_Size,
               Src_Module_Id    => 1,
               Src_Request_Id   => CXX_UInt64_Type'Last,
               Req_Type         => Req_Type,
               Prim_Ptr         => Prim'Address,
               Prim_Size        => Prim'Size / 8,
               CBE_Req_Offset   => CXX_UInt64_Type (Request.Offset (Superblock_Control.Peek_Generated_Req (Obj.SB_Ctrl, Prim))),
               CBE_Req_Tag      => CXX_UInt64_Type (Request.Tag (Superblock_Control.Peek_Generated_Req (Obj.SB_Ctrl, Prim))),
               Last_Secured_Gen => CXX_UInt32_Type (Obj.Superblock.Last_Secured_Generation),
               FT_Root_PBA_Ptr  => Obj.Superblock.Free_Number'Address,
               FT_Root_Gen_Ptr  => Obj.Superblock.Free_Gen'Address,
               FT_Root_Hash_Ptr => Obj.Superblock.Free_Hash'Address,
               FT_Max_Lvl       => CXX_UInt64_Type (Obj.Superblock.Free_Max_Level),
               FT_Degree        => CXX_UInt64_Type (Obj.Superblock.Free_Degree),
               FT_Leaves        => CXX_UInt64_Type (Obj.Superblock.Free_Leafs),
               MT_Root_PBA_Ptr  => Obj.Superblock.Meta_Number'Address,
               MT_Root_Gen_Ptr  => Obj.Superblock.Meta_Gen'Address,
               MT_Root_Hash_Ptr => Obj.Superblock.Meta_Hash'Address,
               MT_Max_Lvl       => CXX_UInt64_Type (Obj.Superblock.Meta_Max_Level),
               MT_Degree        => CXX_UInt64_Type (Obj.Superblock.Meta_Degree),
               MT_Leaves        => CXX_UInt64_Type (Obj.Superblock.Meta_Leafs),
               VBD_Degree       => CXX_UInt64_Type (Obj.Superblock.Degree),
               VBD_Highest_VBA  => CXX_UInt64_Type (Max_VBA (Obj)),
               Rekeying         => (if Obj.Superblock.State = Rekeying then 1 else 0),
               VBA              => CXX_UInt64_Type (Primitive.Block_Number (Prim)),
               Snapshot_Ptr     => Block_Data'Address,
               Snapshots_Degree => CXX_UInt32_Type (Superblock_Control.Peek_Generated_Snapshots_Degree (
                                      Obj.SB_Ctrl, Prim, Obj.Superblock)),
               Current_Gen      => CXX_UInt64_Type (Obj.Cur_Gen),
               Key_ID           => CXX_UInt32_Type (Superblock_Control.Peek_Generated_Key_ID (
                                      Obj.SB_Ctrl, Prim))
            );
            return 1;
         end if;
      end;

      --
      --  SB Control -> Cache
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_Cache_Primitive (Obj.SB_Ctrl);
         Req_Type : CXX_Object_Size_Type := CXX_Object_Size_Type'Last;
      begin
         if Primitive.Valid (Prim) then
            case Primitive.Operation (Prim) is
            when Sync  => Req_Type := 3;
            when others => raise Program_error;
            end case;
            Create_Block_IO_Req (
               Buf_Ptr        => Buf_Ptr,
               Buf_Size       => Buf_Size,
               Src_Module_Id  => 1,
               Src_Request_Id => CXX_UInt64_Type'Last,
               Req_Type       => Req_Type,
               CBE_Req_Offset => 0,
               CBE_Req_Tag    => 0,
               Prim_Ptr       => Prim'Address,
               Prim_Size      => Prim'Size / 8,
               Key_ID         => 0,
               PBA            => CXX_UInt64_Type (Primitive.Block_Number (Prim)),
               VBA            => 0,
               Blk_Count      => 1,
               Blk_Ptr        => System.Null_Address
            );
            return 1;
         end if;
      end;

      --
      --  SB Control -> Crypto
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_Crypto_Primitive (
               Obj.SB_Ctrl);
      begin

         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_Crypto_Add_Key =>

               declare
                  Key_Plaintext : constant Key_Plaintext_Type :=
                     Superblock_Control.Peek_Generated_Key_Plaintext (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Crypto_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 1,
                     CBE_Req_Offset => 0,
                     CBE_Req_Tag    => 0,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         => CXX_UInt32_Type (Key_Plaintext.ID),
                     Key_Plain_Ptr  => Key_Plaintext.Value'Address,
                     PBA            => 0,
                     VBA            => 0,
                     Plain_Blk_Ptr  => System.Null_Address,
                     Cipher_Blk_Ptr => System.Null_Address
                  );
                  return 1;

               end;

            when Primitive.Tag_SB_Ctrl_Crypto_Remove_Key =>

               Create_Crypto_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 2,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         =>
                     CXX_UInt32_Type (
                        Superblock_Control.Peek_Generated_Key_ID (
                           Obj.SB_Ctrl, Prim)),

                  Key_Plain_Ptr  => System.Null_Address,
                  PBA            => 0,
                  VBA            => 0,
                  Plain_Blk_Ptr  => System.Null_Address,
                  Cipher_Blk_Ptr => System.Null_Address
               );
               return 1;

            when others =>

               null;

            end case;
         end if;
      end;

      --
      --  SB Control -> TA
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_TA_Primitive (
               Obj.SB_Ctrl);

      begin
         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_TA_Create_Key =>

               Create_Trust_Anchor_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 1,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_Plain_Ptr  => System.Null_Address,
                  Key_Cipher_Ptr => System.Null_Address,
                  Passphrase_Ptr => System.Null_Address,
                  Hash_Ptr       => System.Null_Address
               );
               return 1;

            when Primitive.Tag_SB_Ctrl_TA_Encrypt_Key =>

               declare
                  Key_Plaintext : constant Key_Value_Plaintext_Type :=
                     Superblock_Control.Peek_Generated_Key_Value_Plaintext (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Trust_Anchor_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 2,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_Plain_Ptr  => Key_Plaintext'Address,
                     Key_Cipher_Ptr => System.Null_Address,
                     Passphrase_Ptr => System.Null_Address,
                     Hash_Ptr       => System.Null_Address
                  );
                  return 1;
               end;

            when Primitive.Tag_SB_Ctrl_TA_Decrypt_Key =>

               declare
                  Key_Ciphertext : constant Key_Value_Ciphertext_Type :=
                     Superblock_Control.Peek_Generated_Key_Value_Ciphertext (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Trust_Anchor_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 3,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_Plain_Ptr  => System.Null_Address,
                     Key_Cipher_Ptr => Key_Ciphertext'Address,
                     Passphrase_Ptr => System.Null_Address,
                     Hash_Ptr       => System.Null_Address
                  );
                  return 1;
               end;

            when Primitive.Tag_SB_Ctrl_TA_Secure_SB =>

               declare
                  Hash : constant Hash_Type :=
                     Superblock_Control.Peek_Generated_Hash (
                        Obj.SB_Ctrl, Prim);
               begin

                  Create_Trust_Anchor_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Src_Module_Id  => 1,
                     Src_Request_Id => CXX_UInt64_Type'Last,
                     Req_Type       => 4,
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_Plain_Ptr  => System.Null_Address,
                     Key_Cipher_Ptr => System.Null_Address,
                     Passphrase_Ptr => System.Null_Address,
                     Hash_Ptr       => Hash'Address
                  );
                  return 1;
               end;

            when Primitive.Tag_SB_Ctrl_TA_Last_SB_Hash =>

               Create_Trust_Anchor_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 5,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_Plain_Ptr  => System.Null_Address,
                  Key_Cipher_Ptr => System.Null_Address,
                  Passphrase_Ptr => System.Null_Address,
                  Hash_Ptr       => System.Null_Address
               );
               return 1;

            when others =>

               null;

            end case;

         end if;

      end;

      --
      --  SB Control -> Block IO
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Superblock_Control.Peek_Generated_Blk_IO_Primitive (
               Obj.SB_Ctrl);
      begin

         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_SB_Ctrl_Blk_IO_Write_SB =>

               Create_Block_IO_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 2,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         => 0,
                  PBA            =>
                     CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                  VBA            => 0,
                  Blk_Count      => 1,
                  Blk_Ptr        =>
                     Superblock_Control.Peek_Generated_Blk_Data_Ptr (
                        Obj.SB_Ctrl, Prim)
               );
               return 1;

            when Primitive.Tag_SB_Ctrl_Blk_IO_Read_SB =>

               Create_Block_IO_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 1,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         => 0,
                  PBA            =>
                     CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                  VBA            => 0,
                  Blk_Count      => 1,
                  Blk_Ptr        =>
                     Superblock_Control.Peek_Generated_Blk_Data_Ptr (
                        Obj.SB_Ctrl, Prim)
               );
               return 1;

            when Primitive.Tag_SB_Ctrl_Blk_IO_Sync =>

               Create_Block_IO_Req (
                  Buf_Ptr        => Buf_Ptr,
                  Buf_Size       => Buf_Size,
                  Src_Module_Id  => 1,
                  Src_Request_Id => CXX_UInt64_Type'Last,
                  Req_Type       => 3,
                  CBE_Req_Offset => 0,
                  CBE_Req_Tag    => 0,
                  Prim_Ptr       => Prim'Address,
                  Prim_Size      => Prim'Size / 8,
                  Key_ID         => 0,
                  PBA            =>
                     CXX_UInt64_Type (Primitive.Block_Number (Prim)),
                  VBA            => 0,
                  Blk_Count      => 1,
                  Blk_Ptr        => System.Null_Address
               );
               return 1;

            when others =>

               raise Program_Error;

            end case;
         end if;
      end;
*/


/* Drop generated_request

      when Primitive.Tag_SB_Ctrl_Crypto_Add_Key |
           Primitive.Tag_SB_Ctrl_Crypto_Remove_Key |
           Primitive.Tag_SB_Ctrl_TA_Create_Key |
           Primitive.Tag_SB_Ctrl_TA_Encrypt_Key |
           Primitive.Tag_SB_Ctrl_TA_Decrypt_Key |
           Primitive.Tag_SB_Ctrl_TA_Secure_SB |
           Primitive.Tag_SB_Ctrl_TA_Last_SB_Hash |
           Primitive.Tag_SB_Ctrl_Blk_IO_Write_SB |
           Primitive.Tag_SB_Ctrl_Blk_IO_Read_SB  |
           Primitive.Tag_SB_Ctrl_Blk_IO_Sync |
           Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA |
           Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA |
           Primitive.Tag_SB_Ctrl_Cache =>

         Superblock_Control.Drop_Generated_Primitive (Obj.SB_Ctrl, Prim);

*/

/* generated request complete


      when
         Primitive.Tag_SB_Ctrl_Crypto_Add_Key |
         Primitive.Tag_SB_Ctrl_Crypto_Remove_Key |
         Primitive.Tag_SB_Ctrl_Cache |
         Primitive.Tag_SB_Ctrl_Blk_IO_Write_SB |
         Primitive.Tag_SB_Ctrl_Blk_IO_Sync |
         Primitive.Tag_SB_Ctrl_Blk_IO_Read_SB |
         Primitive.Tag_SB_Ctrl_TA_Secure_SB |
         Primitive.Tag_SB_Ctrl_VBD_Rkg_Read_VBA =>

         Superblock_Control.Mark_Generated_Prim_Complete (
            Obj.SB_Ctrl, Prim);

      when Primitive.Tag_SB_Ctrl_VBD_Rkg_Write_VBA =>

         declare
            Snap : Snapshot_Type;
         begin
            Snapshot_From_Block_Data (Snap, Snap_Blk_Acc.all, 0);
            Superblock_Control.Mark_Generated_Prim_Complete_Snap (
               Obj.SB_Ctrl, Prim, Snap);
         end;

      when Primitive.Tag_SB_Ctrl_TA_Create_Key |
           Primitive.Tag_SB_Ctrl_TA_Decrypt_Key =>

         Superblock_Control.
            Mark_Generated_Prim_Complete_Key_Value_Plaintext (
               Obj.SB_Ctrl, Prim, Key_Plain_Acc.all);

      when Primitive.Tag_SB_Ctrl_TA_Encrypt_Key =>

         Superblock_Control.
            Mark_Generated_Prim_Complete_Key_Value_Ciphertext (
               Obj.SB_Ctrl, Prim, Key_Cipher_Acc.all);

      when Primitive.Tag_SB_Ctrl_TA_Last_SB_Hash =>

         Superblock_Control.Mark_Generated_Prim_Complete_SB_Hash (
            Obj.SB_Ctrl, Prim, Hash_Acc.all);
*/

/* peek completed request


      Loop_Completed_Prims :
      loop
         Declare_Prim :
         declare
            Prim : constant Primitive.Object_Type :=
               Superblock_Control.Peek_Completed_Primitive (Obj.SB_Ctrl);
         begin
            exit Loop_Completed_Prims when !Primitive.Valid (Prim);

            case Primitive.Tag (Prim) is
            when
               Primitive.Tag_Pool_SB_Ctrl_Read_VBA |
               Primitive.Tag_Pool_SB_Ctrl_Write_VBA |
               Primitive.Tag_Pool_SB_Ctrl_Sync |
               Primitive.Tag_Pool_SB_Ctrl_Deinitialize
            =>

               Request_Pool.Mark_Generated_Primitive_Complete (
                  Obj.Request_Pool_Obj,
                  Pool_Idx_Slot_Content (Primitive.Pool_Idx_Slot (Prim)),
                  Primitive.Success (Prim));

               Superblock_Control.Drop_Completed_Primitive (Obj.SB_Ctrl, Prim);
               Progress := True;

            when Primitive.Tag_Pool_SB_Ctrl_Initialize =>

               Request_Pool.Mark_Generated_Primitive_Complete_SB_State (
                  Obj.Request_Pool_Obj,
                  Pool_Idx_Slot_Content (Primitive.Pool_Idx_Slot (Prim)),
                  Primitive.Success (Prim),
                  Obj.Superblock.State);

               Superblock_Control.Drop_Completed_Primitive (Obj.SB_Ctrl, Prim);
               Progress := True;

            when others =>

               raise Program_Error;

            end case;

         end Declare_Prim;
      end loop Loop_Completed_Prims;
*/
