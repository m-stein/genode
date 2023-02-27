/*
 * \brief  Module for accessing the systems trust anchor
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BLOCK_IO_H_
#define _BLOCK_IO_H_

/* gems includes */
#include <cbe/types.h>

/* cbe tester includes */
#include <module.h>
#include <vfs_utilities.h>

namespace Cbe
{
	class Block_ia;
	class Block_io_request;
	class Block_io_channel;
}

class Cbe::Block_io_request : public Module_request
{
	public:

		enum Type {
			INVALID = 0, READ = 1, WRITE = 2, SYNC = 3, READ_CLIENT_DATA = 4,
			WRITE_CLIENT_DATA = 5 };

	private:

		friend class Block_ia;
		friend class Block_io_channel;

		Type             _type                { INVALID };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE] { 0 };
		Genode::uint64_t _blk_nr              { 0 };
		Genode::uint64_t _blk_count           { 0 };
		Genode::addr_t   _blk_ptr             { 0 };
		bool             _success             { false };

	public:

		Block_io_request() { }

		Block_io_request(unsigned long src_module_id,
		                 unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::size_t    src_module_id,
		                   Genode::size_t    src_request_id,
		                   Genode::size_t    req_type,
		                   void             *prim_ptr,
		                   Genode::size_t    prim_size,
		                   Genode::uint64_t  blk_nr,
		                   Genode::uint64_t  blk_count,
		                   void             *blk_ptr);

		void *prim_ptr() { return (void *)&_prim; }

		Type type() const { return _type; }

		bool success() const { return _success; }

		static char const *type_to_string(Type type);


		/********************
		 ** Module_request **
		 ********************/

		char const *type_name() override { return type_to_string(_type); }
};

class Cbe::Block_io_channel
{
	private:

		friend class Block_ia;

		enum State { INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE };

		State            _state                 { INACTIVE };
		Block_io_request _request               { };
		Vfs::file_offset _nr_of_processed_bytes { 0 };
		Vfs::file_size   _nr_of_remaining_bytes { 0 };
};

class Cbe::Block_ia : public Module
{
	private:

		using Request = Block_io_request;
		using Channel = Block_io_channel;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using file_size = Vfs::file_size;
		using file_offset = Vfs::file_offset;

		enum { NR_OF_CHANNELS = 1 };

		String<32> const  _path;
		Vfs::Env         &_vfs_env;
		Vfs::Vfs_handle  &_vfs_handle               { vfs_open_rw(_vfs_env, _path) };
		Channel           _channels[NR_OF_CHANNELS] { };

		void _execute_read(Channel &channel,
		                   bool    &progress);

		void _execute_write(Channel &channel,
		                    bool    &progress);

		void _execute_sync(Channel &channel,
		                   bool    &progress);


		/************
		 ** Module **
		 ************/

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

	public:

		Block_ia(Vfs::Env               &vfs_env,
		         Genode::Xml_node const &xml_node);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		void execute(bool &) override;
};

#endif /* _BLOCK_IO_H_ */
