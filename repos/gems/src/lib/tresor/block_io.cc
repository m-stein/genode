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
	ASSERT_NEVER_REACHED;
}


void Block_io_channel::_generated_req_completed(State_uint state_uint)
{
	if (!_generated_req_success) {
		error("free tree: request (", *_req_ptr, ") failed because generated request failed)");
		_req_ptr->_success = false;
		_state = COMPLETE;
		return;
	}
	_state = (State)state_uint;
}


void Block_io_channel::_mark_req_failed(bool &progress,
                                char const *str)
{
	error("request failed: failed to ", str);
	_req_ptr->_success = false;
	_state = COMPLETE;
	progress = true;
}


void Block_io_channel::_mark_req_successful(bool &progress)
{
	Request &req { *_req_ptr };
	req._success = true;
	_state = COMPLETE;
	progress = true;
	if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == req._pba)) {
		switch (req._type) {
		case Request::READ:
		case Request::WRITE:
			log("block_io: ", req.type_to_string(req._type), " pba ", req._pba,
			    " data ", req._blk, " hash ", hash(req._blk));
			break;
		case Request::READ_CLIENT_DATA:
		case Request::WRITE_CLIENT_DATA:
			log("block_io: ", req.type_to_string(req._type), " pba ", req._pba,
			    " data ", _blk_buf,
			    " hash ", hash(_blk_buf));
			break;
		default: break;
		}
	}
}


void Block_io_channel::_execute_read(bool &progress)
{
	using Result = Vfs::File_io_service::Read_result;

	Request &req { *_req_ptr };
	switch (_state) {
	case PENDING:

		enum : uint64_t { MAX_FILE_OFFSET = 0x7fffffffffffffff };
		if (req._pba > (size_t)MAX_FILE_OFFSET / (size_t)BLOCK_SIZE) {

			error("request failed: failed to seek file offset, pba: ", req._pba);
			_state = COMPLETE;
			req._success = false;
			progress = true;
			return;
		}
		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 _nr_of_processed_bytes);

		if (!_vfs_handle.fs().queue_read(&_vfs_handle, _nr_of_remaining_bytes)) {
			return;
		}
		_state = IN_PROGRESS;
		progress = true;
		return;

	case IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst {
			(char *)&req._blk + _nr_of_processed_bytes,
			_nr_of_remaining_bytes };

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
				_state = COMPLETE;
				req._success = false;
				progress = true;
				return;
			}
			_nr_of_processed_bytes += nr_of_read_bytes;
			_nr_of_remaining_bytes -= nr_of_read_bytes;

			if (_nr_of_remaining_bytes == 0) {

				_state = COMPLETE;
				req._success = true;
				progress = true;
				return;

			}
			_state = PENDING;
			progress = true;
			return;

		case Result::READ_ERR_IO:
		case Result::READ_ERR_INVALID:

			error("request failed: failed to read from file");
			_state = COMPLETE;
			req._success = false;
			progress = true;
			return;

		default: ASSERT_NEVER_REACHED;
		}
	}
	default: return;
	}
}


void Block_io_channel::_execute_read_client_data(bool &progress)
{
	using Result = Vfs::File_io_service::Read_result;

	Request &req { *_req_ptr };
	switch (_state) {
	case PENDING:

		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 _nr_of_processed_bytes);

		if (!_vfs_handle.fs().queue_read(&_vfs_handle, _nr_of_remaining_bytes)) {
			return;
		}
		_state = IN_PROGRESS;
		progress = true;
		return;

	case IN_PROGRESS:
	{
		size_t nr_of_read_bytes { 0 };

		Byte_range_ptr dst {
			(char *)&_blk_buf + _nr_of_processed_bytes,
			_nr_of_remaining_bytes };

		Result const result {
			_vfs_handle.fs().complete_read(
				&_vfs_handle, dst, nr_of_read_bytes) };

		switch (result) {
		case Result::READ_QUEUED:
		case Result::READ_ERR_WOULD_BLOCK:

			return;

		case Result::READ_OK:

			_nr_of_processed_bytes += nr_of_read_bytes;
			_nr_of_remaining_bytes -= nr_of_read_bytes;

			if (_nr_of_remaining_bytes == 0) {

				_generate_req<Crypto_request>(
					DECRYPT_CLIENT_DATA_COMPLETE, progress, Crypto_request::DECRYPT_CLIENT_DATA, req._client_req_offset,
					req._client_req_tag, req._key_id, *(Key_value *)0, req._pba, req._vba, _blk_buf, _blk_buf);
				return;

			} else {

				_state = PENDING;
				progress = true;
				return;
			}

		case Result::READ_ERR_IO:
		case Result::READ_ERR_INVALID:

			_state = COMPLETE;
			req._success = false;
			progress = true;
			return;

		default:

			class Bad_complete_read_result { };
			throw Bad_complete_read_result { };
		}
	}
	case DECRYPT_CLIENT_DATA_COMPLETE: _mark_req_successful(progress); return;
	default: return;
	}
}


void Block_io_channel::_execute_write_client_data(bool &progress)
{
	using Result = Vfs::File_io_service::Write_result;

	Request &req { *_req_ptr };
	switch (_state) {
	case PENDING:

		_generate_req<Crypto_request>(
			ENCRYPT_CLIENT_DATA_COMPLETE, progress, Crypto_request::ENCRYPT_CLIENT_DATA, req._client_req_offset,
			req._client_req_tag, req._key_id, *(Key_value *)0, req._pba, req._vba, _blk_buf, _blk_buf);
		return;

	case ENCRYPT_CLIENT_DATA_COMPLETE:

		calc_hash(_blk_buf, req._hash);
		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 _nr_of_processed_bytes);

		_state = IN_PROGRESS;
		progress = true;
		return;

	case IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src {
			(char const *)&_blk_buf + _nr_of_processed_bytes,
			_nr_of_remaining_bytes };

		Result const result =
			_vfs_handle.fs().write(
				&_vfs_handle, src, nr_of_written_bytes);

		switch (result) {
		case Result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Result::WRITE_OK:

			_nr_of_processed_bytes += nr_of_written_bytes;
			_nr_of_remaining_bytes -= nr_of_written_bytes;

			if (_nr_of_remaining_bytes == 0) {

				_state = COMPLETE;
				req._success = true;
				progress = true;
				return;

			} else {

				_state = PENDING;
				progress = true;
				return;
			}

		case Result::WRITE_ERR_IO:
		case Result::WRITE_ERR_INVALID:

			_state = COMPLETE;
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


void Block_io_channel::_execute_write(bool &progress)
{
	using Result = Vfs::File_io_service::Write_result;

	Request &req { *_req_ptr };
	switch (_state) {
	case PENDING:

		_vfs_handle.seek(req._pba * BLOCK_SIZE +
		                 _nr_of_processed_bytes);

		_state = IN_PROGRESS;
		progress = true;
		break;

	case IN_PROGRESS:
	{
		size_t nr_of_written_bytes { 0 };

		Const_byte_range_ptr src {
			(char const *)&req._blk + _nr_of_processed_bytes,
			_nr_of_remaining_bytes };

		Result const result =
			_vfs_handle.fs().write(
				&_vfs_handle, src, nr_of_written_bytes);

		switch (result) {
		case Result::WRITE_ERR_WOULD_BLOCK:
			return;

		case Result::WRITE_OK:

			_nr_of_processed_bytes += nr_of_written_bytes;
			_nr_of_remaining_bytes -= nr_of_written_bytes;

			if (_nr_of_remaining_bytes == 0) {

				_state = COMPLETE;
				req._success = true;
				progress = true;
				return;

			} else {

				_state = PENDING;
				progress = true;
				return;
			}

		case Result::WRITE_ERR_IO:
		case Result::WRITE_ERR_INVALID:

			_state = COMPLETE;
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

void Block_io_channel::_execute_sync(bool &progress)
{
	using Result = Vfs::File_io_service::Sync_result;

	Request &req { *_req_ptr };
	switch (_state) {
	case PENDING:

		if (!_vfs_handle.fs().queue_sync(&_vfs_handle)) {
			return;
		}
		_state = IN_PROGRESS;
		progress = true;
		break;;

	case IN_PROGRESS:

		switch (_vfs_handle.fs().complete_sync(&_vfs_handle)) {
		case Result::SYNC_QUEUED:

			return;

		case Result::SYNC_ERR_INVALID:

			req._success = false;
			_state = COMPLETE;
			progress = true;
			return;

		case Result::SYNC_OK:

			req._success = true;
			_state = COMPLETE;
			progress = true;
			return;

		default:

			class Bad_sync_result { };
			throw Bad_sync_result { };
		}

	default: return;
	}
}


void Block_io_channel::execute(bool &progress)
{
	if (!_req_ptr)
		return;

	Request &req { *_req_ptr };
	if (_state == SUBMITTED) {
		_nr_of_processed_bytes = 0;
		_nr_of_remaining_bytes = (size_t)req._blk_count * BLOCK_SIZE;
		_state = PENDING;
	}
	switch (req._type) {
	case Request::READ: _execute_read(progress); break;
	case Request::WRITE: _execute_write(progress); break;
	case Request::SYNC: _execute_sync(progress); break;
	case Request::READ_CLIENT_DATA: _execute_read_client_data(progress); break;
	case Request::WRITE_CLIENT_DATA: _execute_write_client_data(progress); break;
	}
}


void Block_io::execute(bool &progress)
{
	for_each_channel<Channel>([&] (Channel &chan) {
		chan.execute(progress); });
}


void Block_io_channel::_request_submitted(Module_request &mod_req)
{
	_req_ptr = static_cast<Request *>(&mod_req);
	_state = SUBMITTED;
}


Block_io_channel::Block_io_channel(Module_channel_id id, Vfs::Env &vfs_env, Xml_node const &xml_node)
:
	Module_channel { BLOCK_IO, id }, _vfs_env { vfs_env }, _path { xml_node.attribute_value("path", String<32>()) }
{ }


Block_io::Block_io(Vfs::Env &vfs_env, Xml_node const &xml_node)
{
	Module_channel_id id { 0 };
	for (Constructible<Channel> &chan : _channels) {
		chan.construct(id++, vfs_env, xml_node);
		add_channel(*chan);
	}
}
