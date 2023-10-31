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
	friend class Block_io_channel;

	public:

		enum Type { READ, WRITE, SYNC, READ_CLIENT_DATA, WRITE_CLIENT_DATA };

	private:

		Type const _type;
		Request_offset const _client_req_offset;
		Request_tag const _client_req_tag;
		Key_id const _key_id;
		Physical_block_address const _pba;
		Virtual_block_address const _vba;
		Number_of_blocks const _blk_count;
		Block &_blk;
		Hash &_hash;
		bool &_success;

		NONCOPYABLE(Block_io_request);

	public:

		Block_io_request(Module_id, Module_channel_id, Type, Request_offset, Request_tag, Key_id,
		                 Physical_block_address, Virtual_block_address, Number_of_blocks, Block &, Hash &, bool &);

		static char const *type_to_string(Type type);

		void print(Output &out) const override;
};

class Tresor::Block_io_channel : public Module_channel
{
	private:

		using Path = String<128>;
		using Request = Block_io_request;
		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using Sync_result = Vfs::File_io_service::Sync_result;

		enum State {
			REQ_SUBMITTED, QUEUE_READ, SEEK, QUEUE_SYNC, REQ_COMPLETE, ENCRYPT_CLIENT_DATA, ENCRYPT_CLIENT_DATA_COMPLETE,
			DECRYPT_CLIENT_DATA_COMPLETE, WRITE, COMPLETE_READ, COMPLETE_SYNC, REQ_GENERATED };

		State _state { REQ_COMPLETE };
		Vfs::file_offset _num_processed_bytes { 0 };
		size_t _num_remaining_bytes { 0 };
		Block _blk_buf { };
		bool _generated_req_success { false };
		Block_io_request *_req_ptr { };
		Vfs::Env &_vfs_env;
		Path const _path;
		Vfs::Vfs_handle &_vfs_handle { vfs_open_rw(_vfs_env, _path) };

		NONCOPYABLE(Block_io_channel);

		void _generated_req_completed(State_uint) override;

		template <typename REQUEST, typename... ARGS>
		void _generate_req(State_uint state, bool &progress, ARGS &&... args)
		{
			_state = REQ_GENERATED;
			generate_req<REQUEST>(state, progress, args..., _generated_req_success);
		}

		void _request_submitted(Module_request &) override;

		bool _request_complete() override { return _state == REQ_COMPLETE; }

		void _execute_read(bool &);

		void _execute_write(bool &);

		void _execute_read_client_data(bool &);

		void _execute_write_client_data(bool &);

		void _execute_sync(bool &);

		void _mark_req_failed(bool &, char const *);

		void _mark_req_successful(bool &);

		void _reset(State, bool &);

	public:

		Block_io_channel(Module_channel_id, Vfs::Env &, Xml_node const &);

		void execute(bool &);
};

class Tresor::Block_io : public Module
{
	private:

		using Request = Block_io_request;
		using Channel = Block_io_channel;

		Constructible<Channel> _channels[1] { };

		NONCOPYABLE(Block_io);

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

		Block_io(Vfs::Env &, Xml_node const &);

		void execute(bool &) override;
};

#endif /* _TRESOR__BLOCK_IO_H_ */
