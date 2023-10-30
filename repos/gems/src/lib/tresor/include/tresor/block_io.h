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

#ifndef _TRESOR__BLOCK_IO_H_
#define _TRESOR__BLOCK_IO_H_

/* tresor includes */
#include <tresor/types.h>
#include <tresor/module.h>
#include <tresor/vfs_utilities.h>

namespace Tresor {

	class Block_io;
	class Block_io_request;
	class Block_io_channel;
}

class Tresor::Block_io_request : public Module_request
{
	friend class Block_io;
	friend class Block_io_channel;

	public:

		enum Type { READ, WRITE, SYNC, READ_CLIENT_DATA, WRITE_CLIENT_DATA };

	private:

		Type _type;
		Request_offset _client_req_offset;
		Request_tag _client_req_tag;
		Key_id _key_id;
		Physical_block_address _pba;
		Virtual_block_address _vba;
		Number_of_blocks _blk_count;
		Block &_blk;
		Hash &_hash;
		bool &_success;

	public:

		Block_io_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Key_id,
		                 Physical_block_address, Virtual_block_address, Number_of_blocks, Block &, Hash &, bool &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override;
};

class Tresor::Block_io_channel : public Module_channel
{
	private:

		friend class Block_io;

		enum State {
			INACTIVE, SUBMITTED, PENDING, IN_PROGRESS, COMPLETE,
			ENCRYPT_CLIENT_DATA_PENDING,
			ENCRYPT_CLIENT_DATA_IN_PROGRESS,
			ENCRYPT_CLIENT_DATA_COMPLETE,
			DECRYPT_CLIENT_DATA_PENDING,
			DECRYPT_CLIENT_DATA_IN_PROGRESS,
			DECRYPT_CLIENT_DATA_COMPLETE,
			REQ_GENERATED
		};

		State _state { INACTIVE };
		Key_value _dummy_key { };
		Hash _dummy_hash { };
		Vfs::file_offset _nr_of_processed_bytes { 0 };
		size_t _nr_of_remaining_bytes { 0 };
		Block _blk_buf { };
		bool _generated_req_success { false };
		Constructible<Block_io_request> _request { };

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _request_submitted(Module_request &) override { ASSERT_NEVER_REACHED; }

		bool _request_complete() override { return _state == COMPLETE; }

	public:

		Block_io_channel(Module_channel_id id) : Module_channel { BLOCK_IO, id } { }
};

class Tresor::Block_io : public Module
{
	private:

		using Request = Block_io_request;
		using Channel = Block_io_channel;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using file_size = Vfs::file_size;
		using file_offset = Vfs::file_offset;

		enum { NR_OF_CHANNELS = 1 };

		String<32> const _path;
		Vfs::Env &_vfs_env;
		Vfs::Vfs_handle &_vfs_handle { vfs_open_rw(_vfs_env, _path) };
		Constructible<Channel> _channels[1] { };

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

		bool _peek_completed_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_completed_request(Module_request &req) override;

		void execute(bool &) override;

		bool _peek_generated_request(uint8_t *buf_ptr,
		                             size_t   buf_size) override;

		void _drop_generated_request(Module_request &mod_req) override;

		void generated_request_complete(Module_request &req) override;

		bool new_submit_request() override { return false; }

	public:

		struct Read : Request
		{
			Read(Module_id m, Module_channel_id c, Physical_block_address a, Block &b, bool &s)
			: Request(m, c, Request::READ, 0, 0, 0, a, 0, 1, b, *(Hash*)0, s) { }
		};

		struct Write : Request
		{
			Write(Module_id m, Module_channel_id c, Physical_block_address a, Block const &b, bool &s)
			: Request(m, c, Request::WRITE, 0, 0, 0, a, 0, 1, *const_cast<Block*>(&b), *(Hash*)0, s) { }
		};

		struct Sync : Request
		{
			Sync(Module_id m, Module_channel_id c, bool &s)
			: Request(m, c, Request::SYNC, 0, 0, 0, 0, 0, 0, *(Block*)0, *(Hash*)0, s) { }
		};

		struct Write_client_data : Request
		{
			Write_client_data(Module_id m, Module_channel_id c, Physical_block_address p, Virtual_block_address v,
			                  Key_id k, Request_tag t, Request_offset o, Block const &b, Hash &h, bool &s)
			: Request(m, c, Request::WRITE_CLIENT_DATA, o, t, k, p, v, 1, *const_cast<Block*>(&b), h, s) { }
		};

		struct Read_client_data : Request
		{
			Read_client_data(Module_id m, Module_channel_id c, Physical_block_address p, Virtual_block_address v,
			                  Key_id k, Request_tag t, Request_offset o, Block &b, bool &s)
			: Request(m, c, Request::READ_CLIENT_DATA, o, t, k, p, v, 1, b, *(Hash*)0, s) { }
		};

		Block_io(Vfs::Env       &vfs_env,
		         Xml_node const &xml_node);
};

#endif /* _TRESOR__BLOCK_IO_H_ */
