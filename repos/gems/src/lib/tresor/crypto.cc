/*
 * \brief  Module for encrypting/decrypting single data blocks
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
#include <util/construct_at.h>

/* tresor includes */
#include <tresor/crypto.h>
#include <tresor/client_data.h>
#include <tresor/hash.h>

using namespace Tresor;

Crypto_request::Crypto_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                               Request_offset client_req_offset, Request_tag client_req_tag, Key_id key_id,
                               Key_value const &key_plaintext, Physical_block_address pba,
                               Virtual_block_address vba, Block &plaintext_blk, Block &ciphertext_blk, bool &success)
:
	Module_request { src_module_id, src_chan_id, CRYPTO }, _type { type }, _client_req_offset { client_req_offset },
	_client_req_tag { client_req_tag }, _pba { pba }, _vba { vba }, _key_id { key_id }, _key_plaintext { key_plaintext },
	_plaintext_blk { plaintext_blk }, _ciphertext_blk { ciphertext_blk }, _success { success }
{ }


void Crypto_request::print(Output &out) const
{
	Genode::print(out, type_to_string(_type));
	switch (_type) {
	case ADD_KEY:
	case REMOVE_KEY: Genode::print(out, " ", _key_id); break;
	case DECRYPT:
	case ENCRYPT:
	case DECRYPT_CLIENT_DATA:
	case ENCRYPT_CLIENT_DATA: Genode::print(out, " pba ", _pba); break;
	default: break;
	}
}


char const *Crypto_request::type_to_string(Type type)
{
	switch (type) {
	case ADD_KEY: return "add_key";
	case REMOVE_KEY: return "remove_key";
	case ENCRYPT_CLIENT_DATA: return "encrypt_client_data";
	case DECRYPT_CLIENT_DATA: return "decrypt_client_data";
	case ENCRYPT: return "encrypt";
	case DECRYPT: return "decrypt";
	}
	ASSERT_NEVER_REACHED;
}


void Crypto_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("crypto: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		return;
	}
	_state = (State)state_uint;
}


Crypto_channel::Key_directory &Crypto_channel::_lookup_key_dir(Key_id key_id)
{
	for (Key_directory &key_dir : _key_dirs)
		if (key_dir.key_id == key_id)
			return key_dir;
	ASSERT_NEVER_REACHED;
}


void Crypto_channel::_mark_req_failed(bool &progress, char const *str)
{
	error("crypto: request (", *_req_ptr, ") failed at step \"", str, "\"");
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Crypto_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = COMPLETE;
	progress = true;
	if (VERBOSE_WRITE_VBA && req._type == Request::ENCRYPT_CLIENT_DATA)
		log("  encrypt leaf data: plaintext ", _blk, " hash ", hash(_blk),
		    "\n  update branch:\n    ", Branch_lvl_prefix("leaf data: "), req._ciphertext_blk);

	if (VERBOSE_READ_VBA && req._type == Request::DECRYPT_CLIENT_DATA)
		log("    ", Branch_lvl_prefix("leaf data: "), req._ciphertext_blk,
		    "\n  decrypt leaf data: plaintext ", _blk, " hash ", hash(_blk));

	if (VERBOSE_CRYPTO) {
		switch (req._type) {
		case Request::DECRYPT_CLIENT_DATA:
		case Request::ENCRYPT_CLIENT_DATA:
			log("crypto: ", req.type_to_string(req._type), " pba ", req._pba, " vba ", req._vba,
			    " plain ", _blk, " cipher ", req._ciphertext_blk);
			break;
		default: break;
		}
	}
}


void Crypto_channel::_add_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:
	{
		_add_key_handle.seek(0);

		char buf[sizeof(req._key_id) + KEY_SIZE] { };
		memcpy(buf, &req._key_id, sizeof(req._key_id));
		memcpy(buf + sizeof(req._key_id), &req._key_plaintext, KEY_SIZE);

		Const_byte_range_ptr const src(buf, sizeof(buf));
		size_t nr_of_written_bytes { 0 };
		Write_result const write_result {
			_add_key_handle.fs().write(
				&_add_key_handle, src, nr_of_written_bytes) };

		switch (write_result) {
		case Write_result::WRITE_OK:
		{
			Key_directory *key_dir_ptr { nullptr };
			for (Key_directory &key_dir : _key_dirs) {
				if (key_dir.key_id == 0)
					key_dir_ptr = &key_dir;
			}
			if (key_dir_ptr == nullptr) {

				_mark_req_failed(progress, "find unused key dir");
				return;
			}
			key_dir_ptr->key_id = req._key_id;
			key_dir_ptr->encrypt_handle =
				&vfs_open_rw(
					_vfs_env,
					{ _path.string(), "/keys/", req._key_id, "/encrypt" });

			key_dir_ptr->decrypt_handle =
				&vfs_open_rw(
					_vfs_env,
					{ _path.string(), "/keys/", req._key_id, "/decrypt" });

			_mark_req_successful(progress);
			return;
		}
		case Write_result::WRITE_ERR_WOULD_BLOCK:
		case Write_result::WRITE_ERR_INVALID:
		case Write_result::WRITE_ERR_IO:

			_mark_req_failed(progress, "write command");
			return;
		}
		return;
	}
	default:

		return;
	}
}


void Crypto_channel::_remove_key(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:
	{
		_remove_key_handle.seek(0);

		Const_byte_range_ptr src {
			(char const*)&req._key_id, sizeof(req._key_id) };

		size_t nr_of_written_bytes { 0 };
		Write_result const result =
			_remove_key_handle.fs().write(
				&_remove_key_handle, src, nr_of_written_bytes);

		switch (result) {
		case Write_result::WRITE_OK:
		{
			Key_directory &key_dir { _lookup_key_dir(req._key_id) };
			_vfs_env.root_dir().close(key_dir.encrypt_handle);
			key_dir.encrypt_handle = nullptr;
			_vfs_env.root_dir().close(key_dir.decrypt_handle);
			key_dir.decrypt_handle = nullptr;
			key_dir.key_id = 0;

			_mark_req_successful(progress);
			return;
		}
		case Write_result::WRITE_ERR_WOULD_BLOCK:
		case Write_result::WRITE_ERR_INVALID:
		case Write_result::WRITE_ERR_IO:

			_mark_req_failed(progress, "write command");
			return;
		}
	}
	default:

		return;
	}
}


void Crypto_channel::_encrypt_client_data(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:

		_generate_req<Client_data_request>(
			OBTAIN_PLAINTEXT_BLK_COMPLETE, progress, Client_data_request::OBTAIN_PLAINTEXT_BLK,
			req._client_req_offset, req._client_req_tag, req._pba, req._vba, _blk);;
		return;

	case OBTAIN_PLAINTEXT_BLK_COMPLETE:
	{
		if (!_generated_req_success) {

			_mark_req_failed(progress, "obtain plaintext block");
			return;
		}
		_vfs_handle = _lookup_key_dir(req._key_id).encrypt_handle;
		_vfs_handle->seek(req._pba * BLOCK_SIZE);
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr dst {
			(char *)&_blk, BLOCK_SIZE };

		_vfs_handle->fs().write(
			_vfs_handle, dst, nr_of_written_bytes);

		_state = OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case OP_WRITTEN_TO_VFS_HANDLE:
	{
		_vfs_handle->seek(req._pba * BLOCK_SIZE);
		bool success {
			_vfs_handle->fs().queue_read(
				_vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		_state = QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case QUEUE_READ_SUCCEEDED:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst { (char *)&req._ciphertext_blk, BLOCK_SIZE };
		Read_result const result {
			_vfs_handle->fs().complete_read(
				_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			_mark_req_successful(progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(progress, "read ciphertext data");
			return;
		}
	}
	default:

		return;
	}
}


void Crypto_channel::_encrypt(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:
	{
		_vfs_handle = _lookup_key_dir(req._key_id).encrypt_handle;
		_vfs_handle->seek(req._pba * BLOCK_SIZE);
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src { (char *)&req._plaintext_blk, BLOCK_SIZE };

		_vfs_handle->fs().write(
			_vfs_handle, src, nr_of_written_bytes);

		_state = OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case OP_WRITTEN_TO_VFS_HANDLE:
	{
		_vfs_handle->seek(req._pba * BLOCK_SIZE);
		bool success {
			_vfs_handle->fs().queue_read(
				_vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		_state = QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case QUEUE_READ_SUCCEEDED:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst { (char *)&req._ciphertext_blk, BLOCK_SIZE };
		Read_result const result {
			_vfs_handle->fs().complete_read(
				_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			_mark_req_successful(progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(progress, "read ciphertext data");
			return;
		}
	}
	default:

		return;
	}
}


void Crypto_channel::_decrypt(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:
	{
		_vfs_handle = _lookup_key_dir(req._key_id).decrypt_handle;
		_vfs_handle->seek(req._pba * BLOCK_SIZE);

		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src { (char *)&_req_ptr->_ciphertext_blk, BLOCK_SIZE };

		_vfs_handle->fs().write(
			_vfs_handle, src, nr_of_written_bytes);

		_state = OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case OP_WRITTEN_TO_VFS_HANDLE:
	{
		_vfs_handle->seek(req._pba * BLOCK_SIZE);

		bool success {
			_vfs_handle->fs().queue_read(
				_vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		_state = QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case QUEUE_READ_SUCCEEDED:
	{
		size_t nr_of_read_bytes { 0 };
		Byte_range_ptr dst { (char *)&req._plaintext_blk, BLOCK_SIZE };

		Read_result const result {
			_vfs_handle->fs().complete_read(
				_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			_mark_req_successful(progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(progress, "read plaintext data");
			return;
		}
		return;
	}
	default:

		return;
	}
}


void Crypto_channel::_decrypt_client_data(bool &progress)
{
	Request &req { *_req_ptr };
	switch (_state) {
	case SUBMITTED:
	{
		_vfs_handle = _lookup_key_dir(req._key_id).decrypt_handle;
		if (_vfs_handle == nullptr) {
			_mark_req_failed(progress, "lookup key dir");
			return;
		}
		_vfs_handle->seek(req._pba * BLOCK_SIZE);

		size_t nr_of_written_bytes { 0 };
		Const_byte_range_ptr src { (char *)&_req_ptr->_ciphertext_blk, BLOCK_SIZE };

		_vfs_handle->fs().write(
			_vfs_handle, src, nr_of_written_bytes);

		_state = OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case OP_WRITTEN_TO_VFS_HANDLE:
	{
		_vfs_handle->seek(req._pba * BLOCK_SIZE);

		bool success {
			_vfs_handle->fs().queue_read(
				_vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		_state = QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case QUEUE_READ_SUCCEEDED:
	{
		size_t nr_of_read_bytes { 0 };
		Byte_range_ptr dst {
			(char *)&_blk, BLOCK_SIZE };

		Read_result const result {
			_vfs_handle->fs().complete_read(
				_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			_generate_req<Client_data_request>(
				SUPPLY_PLAINTEXT_BLK_COMPLETE, progress, Client_data_request::SUPPLY_PLAINTEXT_BLK,
				req._client_req_offset, req._client_req_tag, req._pba, req._vba, _blk);;
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK: return;
		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID: _mark_req_failed(progress, "read plaintext data"); return;
		}
		return;
	}
	case SUPPLY_PLAINTEXT_BLK_COMPLETE:

		if (!_generated_req_success) {
			_mark_req_failed(progress, "supply plaintext block");
			return;
		}
		_mark_req_successful(progress);
		return;

	default: return;
	}
}


void Crypto_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = SUBMITTED;
}


void Crypto_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	switch (_req_ptr->_type) {
	case Request::ADD_KEY: _add_key(progress); break;
	case Request::REMOVE_KEY: _remove_key(progress); break;
	case Request::DECRYPT: _decrypt(progress); break;
	case Request::ENCRYPT: _encrypt(progress); break;
	case Request::DECRYPT_CLIENT_DATA: _decrypt_client_data(progress); break;
	case Request::ENCRYPT_CLIENT_DATA: _encrypt_client_data(progress); break;
	}
}


void Crypto::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


Crypto_channel::Crypto_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel { CRYPTO, id }, _vfs_env { vfs_env }, _path { xml_node.attribute_value("path", Path()) }
{ }


Crypto::Crypto(Vfs::Env &vfs_env, Xml_node const &xml_node)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}
