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

class Tresor::Block_io
{
	private:

		Vfs::Vfs_handle &_file_handle;
		addr_t _file_user { };

		NONCOPYABLE(Block_io);

	public:

		Block_io(Vfs::Vfs_handle &file_handle) : _file_handle(file_handle) { }

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
