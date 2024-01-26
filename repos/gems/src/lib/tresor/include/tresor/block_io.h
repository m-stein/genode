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
#include <tresor/file.h>

namespace Tresor {

	class Block_io;
	class Block_io_read;
	class Block_io_write;
	class Block_io_sync;
	class Block_io_request;
	class Block_io_channel;
}

class Tresor::Block_io_read
{
	public:

		using Module = Block_io;

		struct Attr
		{
			Physical_block_address const in_pba;
			Block &out_block;
		};

	private:

		enum State { INIT, COMPLETE, READ, READ_OK, FILE_ERR };

		Request_helper<Block_io_read, State> _helper;
		Constructible<File<State> > _file { };

		NONCOPYABLE(Block_io_read);

	public:

		Block_io_read(Attr const &attr) : _helper(*this, attr) { }

		void print(Output &out) const { Genode::print(out, "read pba ", _helper.attr.in_pba); }

		bool execute(Vfs::Vfs_handle &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Block_io_write
{
	public:

		using Module = Block_io;

		struct Attr
		{
			Physical_block_address const in_pba;
			Block const &in_block;
		};

	private:

		enum State { INIT, COMPLETE, WRITE, WRITE_OK, FILE_ERR };

		Request_helper<Block_io_write, State> _helper;
		Constructible<File<State> > _file { };

		NONCOPYABLE(Block_io_write);

	public:

		Block_io_write(Attr const &attr) : _helper(*this, attr) { }

		void print(Output &out) const { Genode::print(out, "write pba ", _helper.attr.in_pba); }

		bool execute(Vfs::Vfs_handle &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Block_io_sync
{
	public:

		using Module = Block_io;

		struct Attr { };

	private:

		enum State { INIT, COMPLETE, SYNC, SYNC_OK, FILE_ERR };

		Request_helper<Block_io_sync, State> _helper;
		Constructible<File<State> > _file { };

		NONCOPYABLE(Block_io_sync);

	public:

		Block_io_sync(Attr const &attr) : _helper(*this, attr) { }

		void print(Output &out) const { Genode::print(out, "sync"); }

		bool execute(Vfs::Vfs_handle &);

		bool complete() const { return _helper.complete(); }
		bool success() const { return _helper.success(); }
};

class Tresor::Block_io_request : public Module_request
{
	friend class Block_io_channel;

	public:

		enum Type { READ, WRITE, SYNC };

	private:

		Type const _type;
		Physical_block_address const _pba;
		Block &_blk;
		bool &_success;

		NONCOPYABLE(Block_io_request);

	public:

		Block_io_request(Module_id, Module_channel_id, Type, Physical_block_address, Block &, bool &);

		static char const *type_to_string(Type);

		void print(Output &out) const override { Genode::print(out, type_to_string(_type), " pba ", _pba); }
};

class Tresor::Block_io_channel : public Module_channel
{
	private:

		using Request = Block_io_request;

		enum State {
			REQ_SUBMITTED, REQ_COMPLETE, CIPHERTEXT_BLK_OBTAINED, PLAINTEXT_BLK_SUPPLIED, REQ_GENERATED,
			READ_OK, WRITE_OK, SYNC_OK, FILE_ERR };

		State _state { REQ_COMPLETE };
		Block _blk { };
		bool _generated_req_success { false };
		Block_io_request *_req_ptr { };
		Vfs::Env &_vfs_env;
		Tresor::Path const _path;
		Read_write_file<State> _file { _state, _vfs_env, _path };

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

		void _read(bool &);

		void _write(bool &);

		void _sync(bool &);

		void _mark_req_failed(bool &, Error_string);

		void _mark_req_successful(bool &);

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

		Vfs::Env &_vfs_env;
		Path const _path;
		Vfs::Vfs_handle &_file_handle;
		addr_t _file_user { };

		NONCOPYABLE(Block_io);

	public:

		struct Read : Request
		{
			Read(Module_id m, Module_channel_id c, Physical_block_address a, Block &b, bool &s)
			: Request(m, c, Request::READ, a, b, s) { }
		};

		struct Write : Request
		{
			Write(Module_id m, Module_channel_id c, Physical_block_address a, Block const &b, bool &s)
			: Request(m, c, Request::WRITE, a, *const_cast<Block*>(&b), s) { }
		};

		struct Sync : Request
		{
			Sync(Module_id m, Module_channel_id c, bool &s)
			: Request(m, c, Request::SYNC, 0, *(Block*)0, s) { }
		};

		Block_io(Vfs::Env &, Xml_node const &, Vfs::Vfs_handle &);

		void execute(bool &) override;

		template <typename REQ>
		bool execute(REQ &req)
		{
			if (!_file_user)
				_file_user = (addr_t)&req;

			if (_file_user != (addr_t)&req)
				return false;

			bool progress = req.execute(_file_handle);
			if (req.complete())
				_file_user = 0;

			return progress;
		}

		static constexpr char const *name() { return "block_io"; }
};

#endif /* _TRESOR__BLOCK_IO_H_ */
