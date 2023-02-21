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

/* cbe tester includes */
#include <crypto.h>
#include <client_data.h>

using namespace Genode;
using namespace Cbe;


/*************************
 ** Crypto_request **
 *************************/

void Crypto_request::create(void     *buf_ptr,
                            size_t    buf_size,
                            size_t    req_type,
                            uint64_t  client_req_offset,
                            uint64_t  client_req_tag,
                            void     *prim_ptr,
                            size_t    prim_size,
                            uint32_t  key_id,
                            void     *key_plaintext_ptr,
                            uint64_t  pba,
                            uint64_t  vba,
                            void     *plaintext_blk_ptr,
                            void     *ciphertext_blk_ptr)
{
	Crypto_request req { CBE_LIBRARA, ~0UL };
	req._type = (Type)req_type;
	req._client_req_offset = client_req_offset;
	req._client_req_tag = client_req_tag;
	if (prim_size > sizeof(req._prim)) {
		error(prim_size, " ", sizeof(req._prim));
		class Bad_size_1 { };
		throw Bad_size_1 { };
	}
	memcpy(&req._prim, prim_ptr, prim_size);
	req._key_id = key_id;
	if (key_plaintext_ptr != nullptr)
		memcpy(
			&req._key_plaintext, key_plaintext_ptr,
			sizeof(req._key_plaintext));

	req._pba = pba;
	req._vba = vba;
	req._plaintext_blk_ptr = (addr_t)plaintext_blk_ptr;
	req._ciphertext_blk_ptr = (addr_t)ciphertext_blk_ptr;

	if (sizeof(req) > buf_size) {
		class Bad_size_0 { };
		throw Bad_size_0 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


void *Crypto_request::result_blk_ptr()
{
	switch (_type) {
	case DECRYPT: return (void *)_plaintext_blk_ptr;
	case ENCRYPT: return (void *)_ciphertext_blk_ptr;
	case INVALID:
	case ADD_KEY:
	case REMOVE_KEY:
	case DECRYPT_CLIENT_DATA:
	case ENCRYPT_CLIENT_DATA:
		break;
	}
	return nullptr;
}


Crypto_request::Crypto_request(unsigned long src_module_id,
                               unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, CRYPTO }
{ }


char const *Crypto_request::type_name()
{
	switch (_type) {
	case INVALID: return "invalid";
	case ADD_KEY: return "add_key";
	case REMOVE_KEY: return "remove_key";
	case ENCRYPT_CLIENT_DATA: return "encrypt_client_data";
	case DECRYPT_CLIENT_DATA: return "decrypt_client_data";
	case ENCRYPT: return "encrypt";
	case DECRYPT: return "decrypt";
	}
	return "?";
}


/*****************
 ** Crypto **
 *****************/

bool Crypto::_peek_generated_request(uint8_t *buf_ptr,
                                     size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel const &chan { _channels[id] };
		Client_data_request::Type cd_req_type {
			chan._state == Channel::OBTAIN_PLAINTEXT_BLK_PENDING ?
			   Client_data_request::OBTAIN_PLAINTEXT_BLK :
			chan._state == Channel::SUPPLY_PLAINTEXT_BLK_PENDING ?
			   Client_data_request::SUPPLY_PLAINTEXT_BLK :
			   Client_data_request::INVALID };

		if (cd_req_type != Client_data_request::INVALID) {

			Request const &req { chan._request };
			Client_data_request const cd_req {
				CRYPTO, id, cd_req_type, req._client_req_offset,
				req._client_req_tag, req._pba, req._vba,
				(addr_t)&chan._blk_buf };

			if (sizeof(cd_req) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &cd_req, sizeof(cd_req));;
			return true;
		}
	}
	return false;
}


void Crypto::_drop_generated_request(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	case Channel::OBTAIN_PLAINTEXT_BLK_PENDING:
		_channels[id]._state = Channel::OBTAIN_PLAINTEXT_BLK_IN_PROGRESS;
		break;
	case Channel::SUPPLY_PLAINTEXT_BLK_PENDING:
		_channels[id]._state = Channel::SUPPLY_PLAINTEXT_BLK_IN_PROGRESS;
		break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


Crypto::Key_directory &Crypto::_lookup_key_dir(uint32_t key_id)
{
	for (Key_directory &key_dir : _key_dirs) {
		if (key_dir.key_id == key_id) {
			return key_dir;
		}
	}
	class Exception_1 { };
	throw Exception_1 { };
}

void Crypto::_mark_req_failed(Channel    &channel,
                              bool       &progress,
                              char const *str)
{
	error("request failed: failed to ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Crypto::_mark_req_successful(Channel &channel,
                                  bool    &progress)
{
	channel._request._success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Crypto::_execute_add_key(Channel &channel,
                              bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:
	{
		_add_key_handle.seek(0);

		char buf[sizeof(req._key_plaintext) + sizeof(req._key_id)] { };
		memcpy(buf, &req._key_id, sizeof(req._key_id));
		memcpy(buf + sizeof(req._key_id), req._key_plaintext,
		       sizeof(req._key_plaintext));

		Vfs::file_size nr_of_written_bytes { 0 };
		Write_result const write_result {
			_add_key_handle.fs().write(
				&_add_key_handle, buf, sizeof(buf), nr_of_written_bytes) };

		switch (write_result) {
		case Write_result::WRITE_OK:
		{
			Key_directory *key_dir_ptr { nullptr };
			for (Key_directory &key_dir : _key_dirs) {
				if (key_dir.key_id == 0)
					key_dir_ptr = &key_dir;
			}
			if (key_dir_ptr == nullptr) {

				_mark_req_failed(channel, progress, "find unused key dir");
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

			_mark_req_successful(channel, progress);
			return;
		}
		case Write_result::WRITE_ERR_WOULD_BLOCK:
		case Write_result::WRITE_ERR_INVALID:
		case Write_result::WRITE_ERR_IO:

			_mark_req_failed(channel, progress, "write command");
			return;
		}
		return;
	}
	default:

		return;
	}
}


void Crypto::_execute_remove_key(Channel &channel,
                                 bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:
	{
		_remove_key_handle.seek(0);

		Vfs::file_size nr_of_written_bytes { 0 };
		Write_result const result =
			_remove_key_handle.fs().write(
				&_remove_key_handle, (char const*)&req._key_id,
				sizeof(req._key_id), nr_of_written_bytes);

		switch (result) {
		case Write_result::WRITE_OK:
		{
			Key_directory &key_dir { _lookup_key_dir(req._key_id) };
			_vfs_env.root_dir().close(key_dir.encrypt_handle);
			key_dir.encrypt_handle = nullptr;
			_vfs_env.root_dir().close(key_dir.decrypt_handle);
			key_dir.decrypt_handle = nullptr;
			key_dir.key_id = 0;

			_mark_req_successful(channel, progress);
			return;
		}
		case Write_result::WRITE_ERR_WOULD_BLOCK:
		case Write_result::WRITE_ERR_INVALID:
		case Write_result::WRITE_ERR_IO:

			_mark_req_failed(channel, progress, "write command");
			return;
		}
	}
	default:

		return;
	}
}


void Crypto::_execute_encrypt_client_data(Channel &channel,
                                          bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:

		channel._state = Channel::OBTAIN_PLAINTEXT_BLK_PENDING;
		progress = true;
		return;

	case Channel::OBTAIN_PLAINTEXT_BLK_COMPLETE:
	{
		if (!channel._generated_req_success) {

			_mark_req_failed(channel, progress, "obtain plaintext block");
			return;
		}
		channel._vfs_handle = _lookup_key_dir(req._key_id).encrypt_handle;
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);
		Vfs::file_size nr_of_written_bytes { 0 };

		channel._vfs_handle->fs().write(
			channel._vfs_handle, (char *)&channel._blk_buf,
			BLOCK_SIZE, nr_of_written_bytes);

		channel._state = Channel::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Channel::OP_WRITTEN_TO_VFS_HANDLE:
	{
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);
		bool success {
			channel._vfs_handle->fs().queue_read(
				channel._vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		channel._state = Channel::QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case Channel::QUEUE_READ_SUCCEEDED:
	{
		Vfs::file_size nr_of_read_bytes { 0 };
		Read_result const result {
			channel._vfs_handle->fs().complete_read(
				channel._vfs_handle, (char *)req._ciphertext_blk_ptr,
				BLOCK_SIZE, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			_mark_req_successful(channel, progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(channel, progress, "read ciphertext data");
			return;
		}
	}
	default:

		return;
	}
}


void Crypto::_execute_encrypt(Channel &channel,
                              bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:
	{
		channel._vfs_handle = _lookup_key_dir(req._key_id).encrypt_handle;
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);
		Vfs::file_size nr_of_written_bytes { 0 };

		channel._vfs_handle->fs().write(
			channel._vfs_handle, (char *)req._plaintext_blk_ptr,
			BLOCK_SIZE, nr_of_written_bytes);

		channel._state = Channel::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Channel::OP_WRITTEN_TO_VFS_HANDLE:
	{
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);
		bool success {
			channel._vfs_handle->fs().queue_read(
				channel._vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		channel._state = Channel::QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case Channel::QUEUE_READ_SUCCEEDED:
	{
		Vfs::file_size nr_of_read_bytes { 0 };
		Read_result const result {
			channel._vfs_handle->fs().complete_read(
				channel._vfs_handle, (char *)&channel._blk_buf,
				BLOCK_SIZE, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			req._ciphertext_blk_ptr = (addr_t)&channel._blk_buf;
			_mark_req_successful(channel, progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(channel, progress, "read ciphertext data");
			return;
		}
	}
	default:

		return;
	}
}


void Crypto::_execute_decrypt(Channel &channel,
                              bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:
	{
		channel._vfs_handle = _lookup_key_dir(req._key_id).decrypt_handle;
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);

		Vfs::file_size nr_of_written_bytes { 0 };
		channel._vfs_handle->fs().write(
			channel._vfs_handle, (char *)channel._request._ciphertext_blk_ptr,
			BLOCK_SIZE, nr_of_written_bytes);

		channel._state = Channel::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Channel::OP_WRITTEN_TO_VFS_HANDLE:
	{
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);

		bool success {
			channel._vfs_handle->fs().queue_read(
				channel._vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		channel._state = Channel::QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case Channel::QUEUE_READ_SUCCEEDED:
	{
		Vfs::file_size nr_of_read_bytes { 0 };
		Read_result const result {
			channel._vfs_handle->fs().complete_read(
				channel._vfs_handle, (char *)&channel._blk_buf,
				BLOCK_SIZE, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			req._plaintext_blk_ptr = (addr_t)&channel._blk_buf;
			_mark_req_successful(channel, progress);
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(channel, progress, "read plaintext data");
			return;
		}
		return;
	}
	default:

		return;
	}
}


void Crypto::_execute_decrypt_client_data(Channel &channel,
                                          bool    &progress)
{
	Request &req { channel._request };
	switch (channel._state) {
	case Channel::SUBMITTED:
	{
		channel._vfs_handle = _lookup_key_dir(req._key_id).decrypt_handle;
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);

		Vfs::file_size nr_of_written_bytes { 0 };
		channel._vfs_handle->fs().write(
			channel._vfs_handle, (char *)channel._request._ciphertext_blk_ptr,
			BLOCK_SIZE, nr_of_written_bytes);

		channel._state = Channel::OP_WRITTEN_TO_VFS_HANDLE;
		progress = true;
		return;
	}
	case Channel::OP_WRITTEN_TO_VFS_HANDLE:
	{
		channel._vfs_handle->seek(req._pba * BLOCK_SIZE);

		bool success {
			channel._vfs_handle->fs().queue_read(
				channel._vfs_handle, BLOCK_SIZE) };

		if (!success)
			return;

		channel._state = Channel::QUEUE_READ_SUCCEEDED;
		progress = true;
		return;
	}
	case Channel::QUEUE_READ_SUCCEEDED:
	{
		Vfs::file_size nr_of_read_bytes { 0 };
		Read_result const result {
			channel._vfs_handle->fs().complete_read(
				channel._vfs_handle, (char *)&channel._blk_buf,
				BLOCK_SIZE, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_OK:

			channel._state = Channel::SUPPLY_PLAINTEXT_BLK_PENDING;
			progress = true;
			return;

		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_ERR_IO:
		case Read_result::READ_ERR_INVALID:

			_mark_req_failed(channel, progress, "read plaintext data");
			return;
		}
		return;
	}
	case Channel::SUPPLY_PLAINTEXT_BLK_COMPLETE:

		if (!channel._generated_req_success) {

			_mark_req_failed(channel, progress, "supply plaintext block");
			return;
		}
		_mark_req_successful(channel, progress);
		return;

	default:

		return;
	}
}


void Crypto::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		switch (channel._request._type) {
		case Request::ADD_KEY:             _execute_add_key(channel, progress);             break;
		case Request::REMOVE_KEY:          _execute_remove_key(channel, progress);          break;
		case Request::DECRYPT:             _execute_decrypt(channel, progress);             break;
		case Request::ENCRYPT:             _execute_encrypt(channel, progress);             break;
		case Request::DECRYPT_CLIENT_DATA: _execute_decrypt_client_data(channel, progress); break;
		case Request::ENCRYPT_CLIENT_DATA: _execute_encrypt_client_data(channel, progress); break;
		default:
			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}


Crypto::Crypto(Vfs::Env       &vfs_env,
               Xml_node const &xml_node)
:
	_vfs_env           { vfs_env },
	_path              { xml_node.attribute_value("path", String<32>()) },
	_add_key_handle    { vfs_open_wo(_vfs_env, { _path.string(), "/add_key" }) },
	_remove_key_handle { vfs_open_wo(_vfs_env, { _path.string(), "/remove_key" }) }
{ }


void Crypto::generated_request_complete(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (_channels[id]._state) {
	case Channel::OBTAIN_PLAINTEXT_BLK_IN_PROGRESS:
		_channels[id]._state = Channel::OBTAIN_PLAINTEXT_BLK_COMPLETE;
		_channels[id]._generated_req_success =
			dynamic_cast<Client_data_request *>(&req)->success();
		break;
	case Channel::SUPPLY_PLAINTEXT_BLK_IN_PROGRESS:
		_channels[id]._state = Channel::SUPPLY_PLAINTEXT_BLK_COMPLETE;
		_channels[id]._generated_req_success =
			dynamic_cast<Client_data_request *>(&req)->success();
		break;
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


bool Crypto::_peek_completed_request(uint8_t *buf_ptr,
                                     size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
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


void Crypto::_drop_completed_request(Module_request &req)
{
	unsigned long id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]._state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]._state = Channel::INACTIVE;
}


bool Crypto::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Crypto::submit_request(Module_request &req)
{
	for (unsigned long id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]._state == Channel::INACTIVE) {
			req.dst_request_id(id);
			_channels[id]._request = *dynamic_cast<Request *>(&req);
			_channels[id]._state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}
