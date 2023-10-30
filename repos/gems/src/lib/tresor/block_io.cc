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
#include <util/construct_at.h>

/* tresor includes */
#include <tresor/crypto.h>
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;


Block_io_request::Block_io_request(Module_id src_module_id, Module_channel_id src_chan_id, Type type,
                                   Request_offset client_req_offset, Request_tag client_req_tag, Key_id key_id,
                                   Physical_block_address pba, Virtual_block_address vba,
                                   Number_of_blocks blk_count, Block &blk, Hash &hash, bool &success)
:
	Module_request { src_module_id, src_chan_id, BLOCK_IO }, _type { type },
	_client_req_offset { client_req_offset }, _client_req_tag { client_req_tag },
	_key_id { key_id }, _pba { pba }, _vba { vba }, _blk_count { blk_count }, _blk { blk },
	_hash { hash }, _success { success }
{ }


void Block_io_request::print(Output &out) const
{
	if (_blk_count > 1)
		Genode::print(out, type_to_string(_type), " pbas ", _pba, "..", _pba + _blk_count - 1);
	else
		Genode::print(out, type_to_string(_type), " pba ", _pba);
}


char const *Block_io_request::type_to_string(Type type)
{
	switch (type) {
	case READ: return "read";
	case WRITE: return "write";
	case SYNC: return "sync";
	case READ_CLIENT_DATA: return "read_client_data";
	case WRITE_CLIENT_DATA: return "write_client_data";
	}
	return "?";
}


bool Block_io::_peek_generated_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (uint32_t id { 0 }; id < NR_OF_CHANNELS; id++) {

		Channel &channel { *_channels[id] };
		Crypto_request::Type crypto_req_type;
		switch (channel._state) {
		case Channel::DECRYPT_CLIENT_DATA_PENDING: crypto_req_type = Crypto_request::DECRYPT_CLIENT_DATA; break;
		case Channel::ENCRYPT_CLIENT_DATA_PENDING: crypto_req_type = Crypto_request::ENCRYPT_CLIENT_DATA; break;
		default: continue;
		}
		Request &req { *channel._request };
		ASSERT(sizeof(Crypto_request) <= buf_size);
		construct_at<Crypto_request>(
			buf_ptr, BLOCK_IO, id, crypto_req_type, req._client_req_offset, req._client_req_tag, req._key_id,
			*(Key_value *)0, req._pba, req._vba, channel._blk_buf, channel._blk_buf, _channels[id]->_generated_req_success);

		return true;
	}
	return false;
}


void Block_io::_drop_generated_request(Module_request &req)
{
	Module_request_id const id { req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Bad_id { };
		throw Bad_id { };
	}
	switch (_channels[id]->_state) {
	case Channel::DECRYPT_CLIENT_DATA_PENDING: _channels[id]->_state = Channel::DECRYPT_CLIENT_DATA_IN_PROGRESS; break;
	case Channel::ENCRYPT_CLIENT_DATA_PENDING: _channels[id]->_state = Channel::ENCRYPT_CLIENT_DATA_IN_PROGRESS; break;
	default: ASSERT_NEVER_REACHED;
	}
}


void Block_io::generated_request_complete(Module_request &mod_req)
{
	Module_request_id const id { mod_req.src_request_id() };
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	switch (_channels[id]->_state) {
	case Channel::DECRYPT_CLIENT_DATA_IN_PROGRESS: _channels[id]->_state = Channel::DECRYPT_CLIENT_DATA_COMPLETE; break;
	case Channel::ENCRYPT_CLIENT_DATA_IN_PROGRESS: _channels[id]->_state = Channel::ENCRYPT_CLIENT_DATA_COMPLETE; break;
	default: ASSERT_NEVER_REACHED;
	}
}


void Block_io::_mark_req_failed(Channel    &channel,
                                bool       &progress,
                                char const *str)
{
	error("request failed: failed to ", str);
	channel._request->_success = false;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Block_io::_mark_req_successful(Channel &channel,
                                    bool    &progress)
{
	channel._request->_success = true;
	channel._state = Channel::COMPLETE;
	progress = true;
}


void Block_io::_execute_read(Channel &channel,
                             bool    &progress)
{
	using Result = Vfs::File_io_service::Read_result;

	Request &req { *channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		enum : uint64_t { MAX_FILE_OFFSET = 0x7fffffffffffffff };
		if (req._pba > (size_t)MAX_FILE_OFFSET / (size_t)BLOCK_SIZE) {

			error("request failed: failed to seek file offset, pba: ", req._pba);
			channel._state = Channel::COMPLETE;
			req._success = false;
			progress = true;
			return;
		}
		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		if (!_vfs_handle.fs().queue_read(&_vfs_handle, channel._nr_of_remaining_bytes)) {
			return;
		}
		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst {
			(char *)&req._blk + channel._nr_of_processed_bytes,
			channel._nr_of_remaining_bytes };

		Result const result {
			_vfs_handle.fs().complete_read(
				&_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Result::READ_QUEUED:
		case Result::READ_ERR_WOULD_BLOCK:

			return;

		case Result::READ_OK:

			if (nr_of_read_bytes == 0) {

				error("request failed: number of read bytes is 0");
				channel._state = Channel::COMPLETE;
				req._success = false;
				progress = true;
				return;
			}
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

			error("request failed: failed to read from file");
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

	Request &req { *channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		if (!_vfs_handle.fs().queue_read(&_vfs_handle, channel._nr_of_remaining_bytes)) {
			return;
		}
		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst {
			(char *)&channel._blk_buf + channel._nr_of_processed_bytes,
			channel._nr_of_remaining_bytes };

		Result const result {
			_vfs_handle.fs().complete_read(
				&_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Result::READ_QUEUED:
		case Result::READ_ERR_WOULD_BLOCK:

			return;

		case Result::READ_OK:

			channel._nr_of_processed_bytes += nr_of_read_bytes;
			channel._nr_of_remaining_bytes -= nr_of_read_bytes;

			if (channel._nr_of_remaining_bytes == 0) {

				channel._generate_req<Crypto_request>(
					Channel::DECRYPT_CLIENT_DATA_COMPLETE, progress, Crypto_request::DECRYPT_CLIENT_DATA, req._client_req_offset,
					req._client_req_tag, req._key_id, *(Key_value *)0, req._pba, req._vba, channel._blk_buf, channel._blk_buf);
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


void Block_io_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_request, ") failed because generated request failed)");
		_request->_success = false;
		_state = COMPLETE;
		return;
	}
	_state = (State)state_uint;
}


void Block_io::_execute_write_client_data(Channel &channel,
                                          bool    &progress)
{
	using Result = Vfs::File_io_service::Write_result;

	Request &req { *channel._request };
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
		calc_hash(channel._blk_buf, req._hash);
		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src {
			(char const *)&channel._blk_buf + channel._nr_of_processed_bytes,
			channel._nr_of_remaining_bytes };

		Result const result =
			_vfs_handle.fs().write(
				&_vfs_handle, src, nr_of_written_bytes);

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

	Request &req { *channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 channel._nr_of_processed_bytes);

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		break;

	case Channel::IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src {
			(char const *)&req._blk + channel._nr_of_processed_bytes,
			channel._nr_of_remaining_bytes };

		Result const result =
			_vfs_handle.fs().write(
				&_vfs_handle, src, nr_of_written_bytes);

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

	Request &req { *channel._request };
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
	for (Constructible<Channel> &channel_con : _channels) {
		Channel &channel { *channel_con };

		if (channel._state == Channel::INACTIVE)
			continue;

		Request &req { *channel._request };
		if (channel._state == Channel::SUBMITTED) {

			uint64_t const nr_of_remaining_bytes {
				req._blk_count * BLOCK_SIZE };

			if (nr_of_remaining_bytes > ~(size_t)0) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			channel._state = Channel::PENDING;
			channel._nr_of_processed_bytes = 0;
			channel._nr_of_remaining_bytes = (size_t)nr_of_remaining_bytes;
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


bool Block_io::_peek_completed_request(uint8_t *buf_ptr,
                                       size_t   buf_size)
{
	for (Constructible<Channel> &channel_con : _channels) {
		Channel &channel { *channel_con };
		if (channel._state == Channel::COMPLETE) {
			Request &req { *channel._request };
			if (sizeof(req) > buf_size) {
				class Exception_1 { };
				throw Exception_1 { };
			}

			construct_at<Request>(buf_ptr, req.src_module_id(), req.src_chan_id(), req._type,
				req._client_req_offset, req._client_req_tag, req._key_id,
				req._pba, req._vba,
				req._blk_count, req._blk, req._hash, req._success);
			(*(Request*)buf_ptr).dst_request_id(req.dst_chan_id());

			if (VERBOSE_BLOCK_IO &&
			    (!VERBOSE_BLOCK_IO_PBA_FILTER ||
			      VERBOSE_BLOCK_IO_PBA == req._pba)) {

				switch (req._type) {
				case Request::READ:
				case Request::WRITE:
				{
					log("block_io: ", req.type_to_string(req._type), " pba ", req._pba,
					    " data ", req._blk, " hash ", hash(req._blk));

					break;
				}
				case Request::READ_CLIENT_DATA:
				case Request::WRITE_CLIENT_DATA:
				{
					log("block_io: ", req.type_to_string(req._type), " pba ", req._pba,
					    " data ", channel._blk_buf,
					    " hash ", hash(channel._blk_buf));

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
	Module_request_id id { 0 };
	id = req.dst_request_id();
	if (id >= NR_OF_CHANNELS) {
		class Exception_1 { };
		throw Exception_1 { };
	}
	if (_channels[id]->_state != Channel::COMPLETE) {
		class Exception_2 { };
		throw Exception_2 { };
	}
	_channels[id]->_state = Channel::INACTIVE;
}


bool Block_io::ready_to_submit_request()
{
	for (Constructible<Channel> &channel_con : _channels) {
		Channel &channel { *channel_con };
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Block_io::submit_request(Module_request &req)
{
	for (Module_request_id id { 0 }; id < NR_OF_CHANNELS; id++) {
		if (_channels[id]->_state == Channel::INACTIVE) {
			req.dst_request_id(id);
			Request &r { *static_cast<Request *>(&req) };
			_channels[id]->_request.construct(r.src_module_id(), r.src_chan_id(),
				r._type,
				r._client_req_offset, r._client_req_tag, r._key_id,
				r._pba, r._vba,
				r._blk_count, r._blk, r._hash, r._success);
			_channels[id]->_request->dst_request_id(id);
			_channels[id]->_state = Channel::SUBMITTED;
			return;
		}
	}
	class Invalid_call { };
	throw Invalid_call { };
}


Block_io::Block_io(Vfs::Env       &vfs_env,
                   Xml_node const &xml_node)
:
	_path    { xml_node.attribute_value("path", String<32> { "" } ) },
	_vfs_env { vfs_env }
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++);
		add_channel(*chan);
	}
}
