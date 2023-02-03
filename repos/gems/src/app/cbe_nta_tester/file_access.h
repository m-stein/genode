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

#ifndef _FILE_ACCESS_H_
#define _FILE_ACCESS_H_

/* local includes */
#include <request_processor.h>

namespace Cbe_tester
{
	class File_access;
	class File_access_request;
	class File_access_channel;
}


class Cbe_tester::File_access_request
{
	public:

		enum Type { INVALID, READ, WRITE };

	private:

		friend class File_access;
		friend class File_access_channel;

		Type             _type                  { INVALID };
		Vfs::Vfs_handle *_file_ptr              { nullptr };
		Genode::off_t    _file_offset           { 0 };
		char            *_buf_ptr               { nullptr };
		Genode::size_t   _buf_size              { 0 };
		Genode::size_t   _nr_of_processed_bytes { 0 };
		bool             _success               { false };

		File_access_request() { }

	public:

		File_access_request(Type             type,
		                    Vfs::Vfs_handle &file,
		                    Genode::off_t    file_offset,
		                    char            *buf_ptr,
		                    Genode::size_t   buf_size);

		Type type() const { return _type; }
		bool success() const { return _success; }
		Genode::size_t nr_of_processed_bytes() const { return _nr_of_processed_bytes; }
};


class Cbe_tester::File_access_channel
{
	private:

		friend class File_access;

		enum State { UNINITIALIZED, IN_PROGRESS, COMPLETED };

		State               _state                 { UNINITIALIZED };
		File_access_request _request               { };
		Genode::size_t      _nr_of_processed_bytes { 0 };

	public:

		bool unused() { return _request._type == File_access_request::INVALID; }

		void use(File_access_request req) { _request = req; }

		bool completed() const { return _state == COMPLETED; }

		bool has_generated_request(File_access_request &) const { return false; }

		bool drop_generated_request() const { return false; }

		bool generated_request_completed(File_access_request const &) const { return false; }

		File_access_request const &request() const { return _request; }
};


class Cbe_tester::File_access
:
	public Genode::Request_processor<File_access,
	                                 File_access_request,
	                                 File_access_channel, 4>
{
	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using Request = File_access_request;
		using Channel = File_access_channel;

		Channel  *_completed_channel_ptr { _channels };
		Vfs::Env &_vfs_env;

		void _call_file_write_once(Channel &channel,
		                           bool    &progress);

		void _execute_write(Channel &channel,
		                    bool    &progress);

		void _execute_read(Channel &channel,
		                   bool    &progress);

	public:

		File_access(Vfs::Env &vfs_env);

		void execute_one_step(bool &progress);
};

#endif /* _FILE_ACCESS_H_ */
