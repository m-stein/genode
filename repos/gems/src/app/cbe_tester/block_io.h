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
		Genode::uint64_t _client_req_offset   { 0 };
		Genode::uint64_t _client_req_tag      { 0 };
		Genode::uint8_t  _prim[PRIM_BUF_SIZE] { 0 };
		Genode::uint32_t _key_id              { 0 };
		Genode::uint64_t _pba                 { 0 };
		Genode::uint64_t _vba                 { 0 };
		Genode::uint64_t _blk_count           { 0 };
		Genode::addr_t   _blk_ptr             { 0 };
		Genode::uint8_t  _hash[HASH_SIZE]     { 0 };
		bool             _success             { false };

	public:

		Block_io_request() { }

		Block_io_request(unsigned long src_module_id,
		                 unsigned long src_request_id);

		static void create(void             *buf_ptr,
		                   Genode::size_t    buf_size,
		                   Genode::uint64_t  src_module_id,
		                   Genode::uint64_t  src_request_id,
		                   Genode::size_t    req_type,
		                   Genode::uint64_t  client_req_offset,
		                   Genode::uint64_t  client_req_tag,
		                   void             *prim_ptr,
		                   Genode::size_t    prim_size,
		                   Genode::uint32_t  key_id,
		                   Genode::uint64_t  pba,
		                   Genode::uint64_t  vba,
		                   Genode::uint64_t  blk_count,
		                   void             *blk_ptr);

		void *prim_ptr() { return (void *)&_prim; }

		void *hash_ptr() { return (void *)&_hash; }

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

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE,
			ENCRYPT_CLIENT_DATA_PENDING,
			ENCRYPT_CLIENT_DATA_IN_PROGRESS,
			ENCRYPT_CLIENT_DATA_COMPLETE,
			DECRYPT_CLIENT_DATA_PENDING,
			DECRYPT_CLIENT_DATA_IN_PROGRESS,
			DECRYPT_CLIENT_DATA_COMPLETE
		};

		State            _state                    { INACTIVE };
		Block_io_request _request                  { };
		Vfs::file_offset _nr_of_processed_bytes    { 0 };
		Vfs::file_size   _nr_of_remaining_bytes    { 0 };
		char             _blk_buf[Cbe::BLOCK_SIZE] { 0 };
		bool             _generated_req_success    { false };
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

		void _execute_read_client_data(Channel &channel,
		                               bool    &progress);

		void _execute_write_client_data(Channel &channel,
		                                bool    &progress);

		void _execute_sync(Channel &channel,
		                   bool    &progress);

		void _mark_req_failed(Channel    &channel,
		                      bool       &progress,
		                      char const *str);

		void _mark_req_successful(Channel &channel,
		                          bool    &progress);


		/************
		 ** Module **
		 ************/

		bool ready_to_submit_request() override;

		void submit_request(Module_request &req) override;

		bool _peek_completed_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(Genode::uint8_t *buf_ptr,
		                             Genode::size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

	public:

		Block_ia(Vfs::Env               &vfs_env,
		         Genode::Xml_node const &xml_node);


		/************
		 ** Module **
		 ************/
};

#endif /* _BLOCK_IO_H_ */
