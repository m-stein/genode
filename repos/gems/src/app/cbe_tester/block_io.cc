/*
 * \brief  Module for accessing the back-end block device
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
#include <block_io.h>
#include <sha256_4k_hash.h>

using namespace Genode;
using namespace Cbe;

enum { VERBOSE_BLOCK_IO = 0 };


/**********************
 ** Block_io_request **
 **********************/

void Block_io_request::create(void       *buf_ptr,
                              size_t      buf_size,
                              uint64_t    src_module_id,
                              uint64_t    src_request_id,
                              size_t      req_type,
                              uint64_t    client_req_offset,
                              uint64_t    client_req_tag,
                              void       *prim_ptr,
                              size_t      prim_size,
                              uint32_t    key_id,
                              uint64_t    pba,
                              uint64_t    vba,
                              uint64_t    blk_count,
                              void       *blk_ptr)
{
	Block_io_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;
	req._client_req_offset = client_req_offset;
	req._client_req_tag = client_req_tag;
	req._pba = pba;
	req._vba = vba;
	req._key_id = key_id;
	if (prim_ptr != nullptr) {
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Exception_1 { };
			throw Exception_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
	}
	req._blk_count = blk_count;
	req._blk_ptr = (addr_t)blk_ptr;

	if (sizeof(req) > buf_size) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	memcpy(buf_ptr, &req, sizeof(req));
}


Block_io_request::Block_io_request(unsigned long src_module_id,
                                   unsigned long src_request_id)
:
	Module_request { src_module_id, src_request_id, BLOCK_IO }
{ }


char const *Block_io_request::type_to_string(Type type)
{
	switch (type) {
	case INVALID: return "invalid";
	case READ: return "read";
	case WRITE: return "write";
	case SYNC: return "sync";
	case READ_CLIENT_DATA: return "read_client_data";
	case WRITE_CLIENT_DATA: return "write_client_data";
	}
	return "?";
}


/**************
 ** Block_io **
 **************/

bool Block_io::_peek_generated_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel const &channel { _channels[id] };
		Crypto_request::Type crypto_req_type {
			channel._state == Channel::DECRYPT_CLIENT_DATA_PENDING ?
			           Crypto_request::DECRYPT_CLIENT_DATA :
			channel._state == Channel::ENCRYPT_CLIENT_DATA_PENDING ?
			           Crypto_request::ENCRYPT_CLIENT_DATA :
			           Crypto_request::INVALID };

		if (crypto_req_type != Crypto_request::INVALID) {

			Request const &req { channel._request };
			Crypto_request::create(
				buf_ptr, buf_size, BLOCK_IO, id, crypto_req_type,
				req._client_req_offset, req._client_req_tag, nullptr, 0,
				req._key_id, nullptr, req._pba, req._vba, nullptr,
				(void *)channel._blk_buf);

			return true;
		}
	}
	return false;
}


void Block_io::_drop_generated_request(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]._state) {
	case Channel::DECRYPT_CLIENT_DATA_PENDING:
		_channels[id]._state = Channel::DECRYPT_CLIENT_DATA_IN_PROGRESS;
		break;
	case Channel::ENCRYPT_CLIENT_DATA_PENDING:
		_channels[id]._state = Channel::ENCRYPT_CLIENT_DATA_IN_PROGRESS;
		break;
	default:
		class Exception_1 { };
		throw Exception_1 { };
	}
}


void Block_io::generated_request_complete(Module_request &req)
{
	unsigned long const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (_channels[id]._state) {
	case Channel::DECRYPT_CLIENT_DATA_IN_PROGRESS:
		_channels[id]._state = Channel::DECRYPT_CLIENT_DATA_COMPLETE;
		_channels[id]._generated_req_success =
			dynamic_cast<Crypto_request *>(&req)->success();
		break;
	case Channel::ENCRYPT_CLIENT_DATA_IN_PROGRESS:
		_channels[id]._state = Channel::ENCRYPT_CLIENT_DATA_COMPLETE;
		_channels[id]._generated_req_success =
			dynamic_cast<Crypto_request *>(&req)->success();
		break;
	default:
		class Exception_2 { };
		throw Exception_2 { };
	}
}


void Block_io::_mark_req_failed(Channel    &channel,
                                bool       &progress,
                                char const *str)
{
	error("request failed: failed to ", str);
	channel._request._success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Block_io::_mark_req_successful(Channel &channel,
                                    bool    &progress)
{
	channel._request._success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Block_io::_execute_read(Channel &channel,
                             bool    &progress)
{
	using Result = Vfs::File_io_service::Read_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._pba * Cbe::BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		if (!_vfs_handle.fs().queue_read(&_vfs_handle, channel._nr_of_remaining_bytes)) {
			return;
		}
		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };

		Result const result {
			_vfs_handle.fs().complete_read(&_vfs_handle,
			                           (char *)req._blk_ptr + channel._nr_of_processed_bytes,
			                           channel._nr_of_remaining_bytes,
			                           nr_of_read_bytes) };

		switch (result) {
		case Result::READ_QUEUED:
		case Result::READ_ERR_WOULD_BLOCK:

			return;

		case Result::READ_OK:

			channel._nr_of_processed_bytes += nr_of_read_bytes;
			channel._nr_of_remaining_bytes -= nr_of_read_bytes;

			if (channel._nr_of_remaining_bytes == 0) {

				channel._state = Channel::COMPLETE;
				req._success = true;
				progress = true;
				return;

			} else {

				channel._state = Channel::PENDING;
				progress = true;
				return;
			}

		case Result::READ_ERR_IO:
		case Result::READ_ERR_INVALID:

			channel._state = Channel::COMPLETE;
			req._success = false;
			progress = true;
			return;

		default:

			class Bad_complete_read_result { };
			throw Bad_complete_read_result { };
		}
	}
	default: return;
	}
}


void Block_io::_execute_read_client_data(Channel &channel,
                                         bool    &progress)
{
	using Result = Vfs::File_io_service::Read_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._pba * Cbe::BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		if (!_vfs_handle.fs().queue_read(&_vfs_handle, channel._nr_of_remaining_bytes)) {
			return;
		}
		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };

		Result const result {
			_vfs_handle.fs().complete_read(&_vfs_handle,
			                           (char *)channel._blk_buf + channel._nr_of_processed_bytes,
			                           channel._nr_of_remaining_bytes,
			                           nr_of_read_bytes) };

		switch (result) {
		case Result::READ_QUEUED:
		case Result::READ_ERR_WOULD_BLOCK:

			return;

		case Result::READ_OK:

			channel._nr_of_processed_bytes += nr_of_read_bytes;
			channel._nr_of_remaining_bytes -= nr_of_read_bytes;

			if (channel._nr_of_remaining_bytes == 0) {

				channel._state = Channel::DECRYPT_CLIENT_DATA_PENDING;
				progress = true;
				return;

			} else {

				channel._state = Channel::PENDING;
				progress = true;
				return;
			}

		case Result::READ_ERR_IO:
		case Result::READ_ERR_INVALID:

			channel._state = Channel::COMPLETE;
			req._success = false;
			progress = true;
			return;

		default:

			class Bad_complete_read_result { };
			throw Bad_complete_read_result { };
		}
	}
	case Channel::DECRYPT_CLIENT_DATA_COMPLETE:

		if (!channel._generated_req_success) {
			_mark_req_failed(channel, progress, "decrypt client data");
			return;
		}
		_mark_req_successful(channel, progress);
		return;

	default: return;
	}
}


void Block_io::_execute_write_client_data(Channel &channel,
                                          bool    &progress)
{
	using Result = Vfs::File_io_service::Write_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		channel._state = Channel::ENCRYPT_CLIENT_DATA_PENDING;
		progress = true;
		return;

	case Channel::ENCRYPT_CLIENT_DATA_COMPLETE:

		if (!channel._generated_req_success) {
			_mark_req_failed(channel, progress, "encrypt client data");
			return;
		}
		sha256_4k_hash((void *)channel._blk_buf, (void *)req._hash);
		_vfs_handle.seek(req._pba * Cbe::BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		file_size nr_of_written_bytes { 0 };

		Result const result =
			_vfs_handle.fs().write(&_vfs_handle,
			                   (char const *)channel._blk_buf + channel._nr_of_processed_bytes,
			                   channel._nr_of_remaining_bytes,
			                   nr_of_written_bytes);

		switch (result) {
		case Result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Result::WRITE_OK:

			channel._nr_of_processed_bytes += nr_of_written_bytes;
			channel._nr_of_remaining_bytes -= nr_of_written_bytes;

			if (channel._nr_of_remaining_bytes == 0) {

				channel._state = Channel::COMPLETE;
				req._success = true;
				progress = true;
				return;

			} else {

				channel._state = Channel::PENDING;
				progress = true;
				return;
			}

		case Result::WRITE_ERR_IO:
		case Result::WRITE_ERR_INVALID:

			channel._state = Channel::COMPLETE;
			req._success = false;
			progress = true;
			return;

		default:

			class Bad_write_result { };
			throw Bad_write_result { };
		}

	}
	default: return;
	}
}


void Block_io::_execute_write(Channel &channel,
                              bool    &progress)
{
	using Result = Vfs::File_io_service::Write_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._pba * Cbe::BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		break;

	case Channel::IN_PROGRESS:
	{
		file_size nr_of_written_bytes { 0 };

		Result const result =
			_vfs_handle.fs().write(&_vfs_handle,
			                   (char const *)req._blk_ptr + channel._nr_of_processed_bytes,
			                   channel._nr_of_remaining_bytes,
			                   nr_of_written_bytes);

		switch (result) {
		case Result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Result::WRITE_OK:

			channel._nr_of_processed_bytes += nr_of_written_bytes;
			channel._nr_of_remaining_bytes -= nr_of_written_bytes;

			if (channel._nr_of_remaining_bytes == 0) {

				channel._state = Channel::COMPLETE;
				req._success = true;
				progress = true;
				return;

			} else {

				channel._state = Channel::PENDING;
				progress = true;
				return;
			}

		case Result::WRITE_ERR_IO:
		case Result::WRITE_ERR_INVALID:

			channel._state = Channel::COMPLETE;
			req._success = false;
			progress = true;
			return;

		default:

			class Bad_write_result { };
			throw Bad_write_result { };
		}

	}
	default: return;
	}
}

void Block_io::_execute_sync(Channel &channel,
                             bool    &progress)
{
	using Result = Vfs::File_io_service::Sync_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		if (!_vfs_handle.fs().queue_sync(&_vfs_handle)) {
			return;
		}
		channel._state = Channel::IN_PROGRESS;
		progress = true;
		break;;

	case Channel::IN_PROGRESS:

		switch (_vfs_handle.fs().complete_sync(&_vfs_handle)) {
		case Result::SYNC_QUEUED:

			return;

		case Result::SYNC_ERR_INVALID:

			req._success = false;
			channel._state = Channel::COMPLETE;
			progress = true;
			return;

		case Result::SYNC_OK:

			req._success = true;
			channel._state = Channel::COMPLETE;
			progress = true;
			return;

		default:

			class Bad_sync_result { };
			throw Bad_sync_result { };
		}

	default: return;
	}
}

void Block_io::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		if (channel._state == Channel::INACTIVE)
			continue;

		Request &req { channel._request };
		if (channel._state == Channel::SUBMITTED) {
			channel._state = Channel::PENDING;
			channel._nr_of_processed_bytes = 0;
			channel._nr_of_remaining_bytes = req._blk_count * Cbe::BLOCK_SIZE;
		}
		switch (req._type) {
		case Request::READ:              _execute_read(channel, progress);              break;
		case Request::WRITE:             _execute_write(channel, progress);             break;
		case Request::SYNC:              _execute_sync(channel, progress);              break;
		case Request::READ_CLIENT_DATA:  _execute_read_client_data(channel, progress);  break;
		case Request::WRITE_CLIENT_DATA: _execute_write_client_data(channel, progress); break;
		default:
			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}


Block_io::Block_io(Vfs::Env       &vfs_env,
                   Xml_node const &xml_node)
:
	_path    { xml_node.attribute_value("path", String<32> { "" } ) },
	_vfs_env { vfs_env }
{ }


bool Block_io::_peek_completed_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::COMPLETE) {
			Request &req { channel._request };
			if (sizeof(req) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			memcpy(buf_ptr, &req, sizeof(req));

			if (VERBOSE_BLOCK_IO) {

				switch (req._type) {
				case Request::READ:
				case Request::WRITE:
				{
					uint8_t hash[HASH_SIZE];
					sha256_4k_hash((void *)req._blk_ptr, (void *)hash);
					uint64_t *blk_ptr { (uint64_t *)req._blk_ptr };
					uint64_t *hash_ptr { (uint64_t *)hash };
					log(req.type_name(), " pba ", req._pba);
					log("  got hash: ",
						Hex(hash_ptr[0], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(hash_ptr[1], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(hash_ptr[2], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(hash_ptr[3], Hex::OMIT_PREFIX, Hex::PAD));
					log("  data: ",
						Hex(blk_ptr[0], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(blk_ptr[1], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(blk_ptr[2], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(blk_ptr[3], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(blk_ptr[4], Hex::OMIT_PREFIX, Hex::PAD), " ",
						Hex(blk_ptr[5], Hex::OMIT_PREFIX, Hex::PAD));
					break;
				}
				default:
					break;
				}
			}
			return true;
		}
	}
	return false;
}


void Block_io::_drop_completed_request(Module_request &req)
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


bool Block_io::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Block_io::submit_request(Module_request &req)
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
