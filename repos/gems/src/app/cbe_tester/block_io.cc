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
#include <block_io.h>

using namespace Genode;
using namespace Cbe;

/*

      --
      --  Block IO -> Crypto
      --
      declare
         Prim : constant Primitive.Object_Type :=
            Block_IO.Peek_Generated_Crypto_Primitive (Obj.IO_Obj);
      begin

         if Primitive.Valid (Prim) then

            case Primitive.Tag (Prim) is
            when Primitive.Tag_Blk_IO_Crypto_Decrypt_And_Supply_Client_Data =>

               declare
                  Req : constant Request.Object_Type :=
                     Block_IO.Peek_Generated_Req (Obj.IO_Obj, Prim);
               begin

                  Create_Crypto_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Req_Type       => 5,
                     CBE_Req_Offset => CXX_UInt64_Type (Request.Offset (Req)),
                     CBE_Req_Tag    => CXX_UInt64_Type (Request.Tag (Req)),
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         =>
                        CXX_UInt32_Type (
                           Block_IO.Peek_Generated_Key_ID (Obj.IO_Obj, Prim)),

                     Key_Plain_Ptr  => System.Null_Address,
                     PBA            =>
                        CXX_UInt64_Type (Primitive.Block_Number (Prim)),

                     VBA            =>
                        CXX_UInt64_Type (
                           Block_IO.Peek_Generated_VBA (Obj.IO_Obj, Prim)),

                     Plain_Blk_Ptr  => System.Null_Address,
                     Cipher_Blk_Ptr =>
                        Blk_IO_Buf (
                           Block_IO.Peek_Generated_Data_Index (
                              Obj.IO_Obj, Prim)
                        )'Address
                  );
                  return 1;

               end;

            when Primitive.Tag_Blk_IO_Crypto_Obtain_And_Encrypt_Client_Data =>

               declare
                  Req : constant Request.Object_Type :=
                     Block_IO.Peek_Generated_Req (Obj.IO_Obj, Prim);
               begin

                  Create_Crypto_Req (
                     Buf_Ptr        => Buf_Ptr,
                     Buf_Size       => Buf_Size,
                     Req_Type       => 6,
                     CBE_Req_Offset => CXX_UInt64_Type (Request.Offset (Req)),
                     CBE_Req_Tag    => CXX_UInt64_Type (Request.Tag (Req)),
                     Prim_Ptr       => Prim'Address,
                     Prim_Size      => Prim'Size / 8,
                     Key_ID         =>
                        CXX_UInt32_Type (
                           Block_IO.Peek_Generated_Key_ID (Obj.IO_Obj, Prim)),

                     Key_Plain_Ptr  => System.Null_Address,
                     PBA            =>
                        CXX_UInt64_Type (Primitive.Block_Number (Prim)),

                     VBA            =>
                        CXX_UInt64_Type (
                           Block_IO.Peek_Generated_VBA (Obj.IO_Obj, Prim)),

                     Plain_Blk_Ptr  => System.Null_Address,
                     Cipher_Blk_Ptr =>
                        Blk_IO_Buf (
                           Block_IO.Peek_Generated_Data_Index (
                              Obj.IO_Obj, Prim)
                        )'Address
                  );
                  return 1;

               end;

            when others =>

               null;

            end case;

         end if;

      end;
*/


/**************************
 ** Block_io_request **
 **************************/

void Block_io_request::create(void       *buf_ptr,
                              size_t      buf_size,
                              size_t      src_module_id,
                              size_t      src_request_id,
                              size_t      req_type,
                              void       *prim_ptr,
                              size_t      prim_size,
                              uint64_t    blk_nr,
                              uint64_t    blk_count,
                              void       *blk_ptr)
{
	Block_io_request req { src_module_id, src_request_id };
	req._type = (Type)req_type;
	if (prim_ptr != nullptr) {
		if (prim_size > sizeof(req._prim)) {
			error(prim_size, " ", sizeof(req._prim));
			class Exception_1 { };
			throw Exception_1 { };
		}
		memcpy(&req._prim, prim_ptr, prim_size);
	}
	req._blk_nr = blk_nr;
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
	}
	return "?";
}


/**************
 ** Block_io **
 **************/

void Block_ia::_execute_read(Channel &channel,
                             bool    &progress)
{
	using Result = Vfs::File_io_service::Read_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._blk_nr * Cbe::BLOCK_SIZE +
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

void Block_ia::_execute_write(Channel &channel,
                              bool    &progress)
{
	using Result = Vfs::File_io_service::Write_result;

	Request &req { channel._request };
	switch (channel._state) {
	case Channel::PENDING:

		_vfs_handle.seek(req._blk_nr * Cbe::BLOCK_SIZE +
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

void Block_ia::_execute_sync(Channel &channel,
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

void Block_ia::execute(bool &progress)
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
		case Request::READ:  _execute_read(channel, progress);  break;
		case Request::WRITE: _execute_write(channel, progress); break;
		case Request::SYNC:  _execute_sync(channel, progress);  break;
		default:
			class Exception_1 { };
			throw Exception_1 { };
		}
	}
}


Block_ia::Block_ia(Vfs::Env       &vfs_env,
                   Xml_node const &xml_node)
:
	_path    { xml_node.attribute_value("path", String<32> { "" } ) },
	_vfs_env { vfs_env }
{ }


bool Block_ia::_peek_completed_request(uint8_t *buf_ptr,
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


void Block_ia::_drop_completed_request(Module_request &req)
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


bool Block_ia::ready_to_submit_request()
{
	for (Channel &channel : _channels) {
		if (channel._state == Channel::INACTIVE)
			return true;
	}
	return false;
}

void Block_ia::submit_request(Module_request &req)
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
