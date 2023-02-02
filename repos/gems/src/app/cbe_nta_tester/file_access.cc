/*
 * \brief  VFS file access in the form of an async request processor
 * \author Martin Stein
 * \date   2023-02-02
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* os includes */
#include <os/vfs.h>

/* local includes */
#include <file_access.h>

using namespace Cbe_tester;
using namespace Genode;
using namespace Vfs;


/*************************
 ** File_access_request **
 *************************/

File_access_request::File_access_request(Type             type,
                                         Vfs::Vfs_handle &file,
                                         Genode::off_t    file_offset,
                                         char            *buf_ptr,
                                         Genode::size_t   buf_size)
:
	_type        { type },
	_file_ptr    { &file },
	_file_offset { file_offset },
	_buf_ptr     { buf_ptr },
	_buf_size    { buf_size }
{ }


/*****************
 ** File_access **
 *****************/

File_access::File_access(Vfs::Env &vfs_env)
:
	_vfs_env { vfs_env }
{ }


void File_access::execute(bool &progress)
{
	for (Channel &channel : _channels) {

		switch (channel._request._type) {
		case Request::READ:    _execute_read(channel, progress);  break;
		case Request::WRITE:   _execute_write(channel, progress); break;
		case Request::INVALID:                                    break;
		}
	}
}


void File_access::_execute_read(Channel &channel,
                                bool    &progress)
{
	Request &req { channel._request };

	switch (channel._state) {
	case Channel::UNINITIALIZED:

		req._file_ptr->seek(req._file_offset);

		if (!req._file_ptr->fs().queue_read(req._file_ptr, req._buf_size))
			return;

		channel._state = Channel::IN_PROGRESS;
		progress = true;
		return;

	case Channel::IN_PROGRESS:
	{
		file_size nr_of_read_bytes { 0 };
		Read_result const result {
			req._file_ptr->fs().complete_read(
				req._file_ptr, req._buf_ptr, req._buf_size, nr_of_read_bytes) };

		switch (result) {
		case Read_result::READ_QUEUED:
		case Read_result::READ_ERR_WOULD_BLOCK:

			return;

		case Read_result::READ_OK:

			req._nr_of_processed_bytes = nr_of_read_bytes;
			req._success = true;
			channel._state = Channel::COMPLETED;
			progress = true;
			return;

		case Read_result::READ_ERR_INVALID:
		case Read_result::READ_ERR_IO:

			req._success = false;
			channel._state = Channel::COMPLETED;
			progress = true;
			return;
		}
	}
	case Channel::COMPLETED:

		return;
	}
}


void File_access::_call_file_write_once(Channel &channel,
                                        bool    &progress)
{
	using Write_result = Vfs::File_io_service::Write_result;

	Request &req { channel._request };

	file_size nr_of_written_bytes { 0 };
	Write_result const result {
		req._file_ptr->fs().write(
			req._file_ptr,
			req._buf_ptr + channel._nr_of_processed_bytes,
			req._buf_size - channel._nr_of_processed_bytes,
			nr_of_written_bytes) };

	switch (result) {
	case Write_result::WRITE_ERR_WOULD_BLOCK:

		return;

	case Write_result::WRITE_OK:

		nr_of_written_bytes =
			min(req._buf_size - channel._nr_of_processed_bytes,
			    nr_of_written_bytes);

		channel._nr_of_processed_bytes += nr_of_written_bytes;

		if (channel._nr_of_processed_bytes < req._buf_size) {

			channel._state = Channel::IN_PROGRESS;
			req._file_ptr->advance_seek(nr_of_written_bytes);

		} else {

			req._nr_of_processed_bytes = nr_of_written_bytes;
			req._success = true;
			channel._state = Channel::COMPLETED;
		}
		progress = true;
		return;

	case Write_result::WRITE_ERR_INVALID:
	case Write_result::WRITE_ERR_IO:

		req._success = false;
		channel._state = Channel::COMPLETED;
		progress = true;
		return;
	}
}


void File_access::_execute_write(Channel &channel,
                                 bool    &progress)
{
	switch (channel._state) {
	case Channel::UNINITIALIZED:

		channel._nr_of_processed_bytes = 0;
		_call_file_write_once(channel, progress);
		return;

	case Channel::IN_PROGRESS:

		_call_file_write_once(channel, progress);
		return;

	case Channel::COMPLETED:

		return;
	}
}
