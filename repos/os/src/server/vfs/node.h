/*
 * \brief  Internal nodes of VFS server
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2016-03-29
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS__NODE_H_
#define _VFS__NODE_H_

/* Genode includes */
#include <file_system/node.h>
#include <vfs/file_system.h>
#include <os/path.h>
#include <base/id_space.h>

/* Local includes */
#include "assert.h"

namespace Vfs_server {

	using namespace File_system;
	using namespace Vfs;

	typedef Vfs::File_io_service::Write_result Write_result;
	typedef Vfs::File_io_service::Read_result Read_result;
	typedef Vfs::File_io_service::Sync_result Sync_result;


	class Node;
	class Io_node;
	class Watch_node;
	class Directory;
	class File;
	class Symlink;

	typedef Genode::Id_space<Node> Node_space;

	struct Session_io_handler : Interface
	{
		virtual void handle_node_io(Io_node &node) = 0;
		virtual void handle_node_watch(Watch_node &node) = 0;
	};

	/* Vfs::MAX_PATH is shorter than File_system::MAX_PATH */
	enum { MAX_PATH_LEN = Vfs::MAX_PATH_LEN };

	typedef Genode::Path<MAX_PATH_LEN> Path;

	typedef Genode::Allocator::Out_of_memory Out_of_memory;

	/**
	 * Type trait for determining the node type for a given handle type
	 */
	template<typename T> struct Node_type;
	template<> struct Node_type<Node_handle>    { typedef Io_node    Type; };
	template<> struct Node_type<Dir_handle>     { typedef Directory  Type; };
	template<> struct Node_type<File_handle>    { typedef File       Type; };
	template<> struct Node_type<Symlink_handle> { typedef Symlink    Type; };
	template<> struct Node_type<Watch_handle>   { typedef Watch_node Type; };

	/**
	 * Type trait for determining the handle type for a given node type
	 */
	template<typename T> struct Handle_type;
	template<> struct Handle_type<Io_node>   { typedef Node_handle    Type; };
	template<> struct Handle_type<Directory> { typedef Dir_handle     Type; };
	template<> struct Handle_type<File>      { typedef File_handle    Type; };
	template<> struct Handle_type<Symlink>   { typedef Symlink_handle Type; };
	template<> struct Handle_type<Watch>     { typedef Watch_handle   Type; };

	/*
	 * Note that the file objects are created at the
	 * VFS in the local node constructors, this is to
	 * ensure that Out_of_ram is thrown before
	 * the VFS is modified.
	 */
}

class Vfs_server::Node : public  File_system::Node_base,
                         private Node_space::Element
{
	public:

		enum Op_state { IDLE, READ_QUEUED, SYNC_QUEUED };

	private:

		/*
		 * Noncopyable
		 */
		Node(Node const &);
		Node &operator = (Node const &);

		Path const _path;

	protected:

		/**
		 * I/O handler for session context
		 */
		Session_io_handler &_session_io_handler;

	public:

		Node(Node_space &space, char const *node_path,
		     Session_io_handler &io_handler)
		:
			Node_space::Element(*this, space),
			_path(node_path),
			_session_io_handler(io_handler)
		{ }

		virtual ~Node() { }

		using Node_space::Element::id;

		char const *path() const { return _path.base(); }

		/**
		 * Print for debugging
		 */
		void print(Genode::Output &out) const {
			out.out_string(_path.base()); }
};

class Vfs_server::Io_node : public  Vfs_server::Node,
                            private Vfs::Vfs_handle::Context
{
	public:

		enum Op_state { IDLE, READ_QUEUED, SYNC_QUEUED };

	private:

		/*
		 * Noncopyable
		 */
		Io_node(Io_node const &);
		Io_node &operator = (Io_node const &);

		Mode const _mode;

		bool _notify_read_ready = false;

	protected:

		Vfs::Vfs_handle::Context &context() { return *this; }

		Vfs::Vfs_handle *_handle { nullptr };
		Op_state         op_state { Op_state::IDLE };

		Read_result _read(char *dst, size_t len, seek_off_t seek_offset, file_size &out_count)
		{
			Read_result out_result = Read_result::READ_ERR_INVALID;
			_handle->seek(seek_offset);

			switch (op_state) {
			case Op_state::IDLE:

				if (!_handle->fs().queue_read(_handle, len))
					return Read_result::READ_QUEUED;

				/* fall through */

			case Op_state::READ_QUEUED:
				out_result = _handle->fs().complete_read(
					_handle, dst, len, out_count);

				switch (out_result) {
				case Read_result::READ_ERR_INVALID:
				case Read_result::READ_ERR_IO:
					out_count = 0;
					/* fall through */
				case Read_result::READ_OK:
					op_state = Op_state::IDLE;
					/* FIXME revise error handling */
					return out_result;

				case Read_result::READ_QUEUED:
					op_state = Op_state::READ_QUEUED;
					return Read_result::READ_QUEUED;
				}
				break;

			case Op_state::SYNC_QUEUED:
				return Read_result::READ_QUEUED;
			}

			return out_result;
		}

		Write_result _write(char const *src, size_t len,
		                    seek_off_t seek_offset, file_size &out_len)
		{
			_handle->seek(seek_offset);

			Write_result res = _handle->fs().write(_handle, src, len, out_len);

			if (out_len)
				mark_as_updated();

			return res;
		}

	public:

		Io_node(Node_space &space, char const *node_path, Mode node_mode,
		        Session_io_handler &io_handler)
		: Node(space, node_path, io_handler), _mode(node_mode) { }

		virtual ~Io_node() { }

		using Node_space::Element::id;

		static Io_node &node_by_context(Vfs::Vfs_handle::Context &context)
		{
			return static_cast<Io_node &>(context);
		}

		Mode mode() const { return _mode; }

		virtual Read_result read(char * /* dst */, size_t /* len */,
		                         seek_off_t /* seek */, file_size& /* out */)
		{ return Read_result::READ_ERR_INVALID; }

		virtual Write_result write(char const * /* src */, size_t /* len */,
		                     seek_off_t /* seek */, file_size& /* out */) {
			return Write_result::WRITE_ERR_INVALID; }

		bool read_ready() { return _handle->fs().read_ready(_handle); }

		/**
		 * The global handler has drawn an association from an I/O
		 * context and this open node, now process the event at the
		 * session for this node.
		 */
		void handle_io_response() {
			_session_io_handler.handle_node_io(*this); }

		void notify_read_ready(bool requested)
		{
			if (requested)
				_handle->fs().notify_read_ready(_handle);
			_notify_read_ready = requested;
		}

		bool notify_read_ready() const { return _notify_read_ready; }

		Sync_result sync()
		{
			switch (op_state) {
			case Op_state::IDLE:

				if (!_handle->fs().queue_sync(_handle))
					return Sync_result::SYNC_QUEUED;

				/* fall through */

			case Op_state::SYNC_QUEUED: {
				Sync_result out_result = _handle->fs().complete_sync(_handle);
				op_state = (out_result == Sync_result::SYNC_QUEUED)
					? Op_state::SYNC_QUEUED : Op_state::IDLE;
				return out_result;
			}

			case Op_state::READ_QUEUED:
				return Sync_result::SYNC_QUEUED;
			}

			return Sync_result::SYNC_ERR_INVALID;
		}
};


class Vfs_server::Watch_node final : public  Vfs_server::Node,
                                     private Vfs::Vfs_watch_handle::Context
{
	private:

		/*
		 * Noncopyable
		 */
		Watch_node(Watch_node const &);
		Watch_node &operator = (Watch_node const &);

		Vfs::Vfs_watch_handle &_watch_handle;

	public:

		Watch_node(Node_space &space,  char const *path,
		           Vfs::Vfs_watch_handle &handle,
		           Session_io_handler &io_handler)
		:
			Node(space, path, io_handler),
			_watch_handle(handle)
		{
			/*
			 * set the context so this Watch object
			 * is passed back thru the Io_handler
			 */
			_watch_handle.context(this);
		}

		~Watch_node()
		{
			_watch_handle.context((Vfs::Vfs_watch_handle::Context*)~0ULL);
			_watch_handle.close();
		}

		static Watch_node &node_by_context(Vfs::Vfs_watch_handle::Context &context)
		{
			return static_cast<Watch_node &>(context);
		}

		/**
		 * The global handler has drawn an association from a watch
		 * context and this open node, now process the event at the
		 * session for this node.
		 */
		void handle_watch_response() {
			_session_io_handler.handle_node_watch(*this); }
};


struct Vfs_server::Symlink : Io_node
{
	Symlink(Node_space         &space,
	        Vfs::File_system   &vfs,
	        Genode::Allocator  &alloc,
	        Session_io_handler &node_io_handler,
	        char       const   *link_path,
	        Mode                mode,
	        bool                create)
	: Io_node(space, link_path, mode, node_io_handler)
	{
		assert_openlink(vfs.openlink(link_path, create, &_handle, alloc));
		_handle->context = &context();
	}

	~Symlink() { _handle->close(); }

	/********************
	 ** Node interface **
	 ********************/

	Read_result read(char *dst, size_t len, seek_off_t seek_offset, file_size &out)
	{
		if (seek_offset != 0) {
			/* partial read is not supported */
			return Read_result::READ_ERR_INVALID;
		}

		return _read(dst, len, 0, out);
	}

	Write_result write(char const *src, size_t const len,
	                   seek_off_t, file_size &out_count) override
	{
		if (len > MAX_PATH_LEN)
			return Write_result::WRITE_ERR_INVALID;

		/* ensure symlink gets something null-terminated */
		Genode::String<MAX_PATH_LEN+1> target(Genode::Cstring(src, len));
		size_t const target_len = target.length()-1;

		Write_result res = _handle->fs().write(
			_handle, target.string(), target_len, out_count);
		if (out_count) {
			mark_as_updated();
			notify_listeners();
		}
		return res;
	}
};


class Vfs_server::File : public Io_node
{
	private:

		/*
		 * Noncopyable
		 */
		File(File const &);
		File &operator = (File const &);

		char const *_leaf_path = nullptr; /* offset pointer to Node::_path */

	public:

		File(Node_space         &space,
		     Vfs::File_system   &vfs,
		     Genode::Allocator  &alloc,
		     Session_io_handler &node_io_handler,
		     char       const   *file_path,
		     Mode                fs_mode,
		     bool                create)
		:
			Io_node(space, file_path, fs_mode, node_io_handler)
		{
			unsigned vfs_mode =
				(fs_mode-1) | (create ? Vfs::Directory_service::OPEN_MODE_CREATE : 0);

			assert_open(vfs.open(file_path, vfs_mode, &_handle, alloc));
			_leaf_path       = vfs.leaf_path(path());
			_handle->context = &context();
		}

		~File() { _handle->close(); }

		Read_result read(char *dst, size_t len, seek_off_t seek_offset, file_size &out_count) override
		{
			if (seek_offset == (seek_off_t)SEEK_TAIL) {
				typedef Directory_service::Stat_result Result;
				Vfs::Directory_service::Stat st;

				/* if stat fails, try and see if the VFS will seek to the end */
				seek_offset = (_handle->ds().stat(_leaf_path, st) == Result::STAT_OK) ?
					((len < st.size) ? (st.size - len) : 0) : (seek_off_t)SEEK_TAIL;
			}

			return _read(dst, len, seek_offset, out_count);
		}

		Write_result write(char const *src, size_t len,
		                   seek_off_t seek_offset, file_size &out_count) override
		{
			if (seek_offset == (seek_off_t)SEEK_TAIL) {
				typedef Directory_service::Stat_result Result;
				Vfs::Directory_service::Stat st;

				/* if stat fails, try and see if the VFS will seek to the end */
				seek_offset = (_handle->ds().stat(_leaf_path, st) == Result::STAT_OK) ?
					st.size : (seek_off_t)SEEK_TAIL;
			}

			return _write(src, len, seek_offset, out_count);
		}

		void truncate(file_size_t size)
		{
			assert_truncate(_handle->fs().ftruncate(_handle, size));
			mark_as_updated();
		}
};


struct Vfs_server::Directory : Io_node
{
	Directory(Node_space         &space,
	          Vfs::File_system   &vfs,
	          Genode::Allocator  &alloc,
	          Session_io_handler &node_io_handler,
	          char const         *dir_path,
	          bool                create)
	: Io_node(space, dir_path, READ_ONLY, node_io_handler)
	{
		assert_opendir(vfs.opendir(dir_path, create, &_handle, alloc));
		_handle->context = &context();
	}

	~Directory() { _handle->close(); }

	Node_space::Id file(Node_space        &space,
	                    Vfs::File_system  &vfs,
	                    Genode::Allocator &alloc,
	                    char        const *file_path,
	                    Mode               mode,
	                    bool               create)
	{
		Path subpath(file_path, path());
		char const *path_str = subpath.base();

		File *file;
		try {
			file = new (alloc) File(space, vfs, alloc,
			                        _session_io_handler,
			                        path_str, mode, create);
		} catch (Out_of_memory) { throw Out_of_ram(); }

		if (create)
			mark_as_updated();
		return file->id();
	}

	Node_space::Id symlink(Node_space        &space,
	                       Vfs::File_system  &vfs,
	                       Genode::Allocator &alloc,
	                       char        const *link_path,
	                       Mode               mode,
	                       bool               create)
	{
		Path subpath(link_path, path());
		char const *path_str = subpath.base();

		Symlink *link;
		try { link = new (alloc) Symlink(space, vfs, alloc,
		                                 _session_io_handler,
		                                 path_str, mode, create); }
		catch (Out_of_memory) { throw Out_of_ram(); }
		if (create)
			mark_as_updated();
		return link->id();
	}


	/********************
	 ** Node interface **
	 ********************/

	Read_result read(char *dst, size_t len,
	                 seek_off_t seek_offset, file_size &out_count) override
	{
		Read_result out_result = Read_result::READ_ERR_INVALID;

		Directory_service::Dirent vfs_dirent;
		size_t blocksize = sizeof(File_system::Directory_entry);

		unsigned index = (seek_offset / blocksize);

		size_t remains = len;

		while (remains >= blocksize) {
			file_size count = 0;

			out_result = _read((char*)&vfs_dirent, sizeof(vfs_dirent),
			          index * sizeof(vfs_dirent), count);
			remains -= count;
			out_count += count;

			if ((count < sizeof(vfs_dirent)) ||
			    (vfs_dirent.type == Vfs::Directory_service::DIRENT_TYPE_END))
				break;

			File_system::Directory_entry *fs_dirent = (Directory_entry *)dst;
			fs_dirent->inode = vfs_dirent.fileno;
			switch (vfs_dirent.type) {
			case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY:
				fs_dirent->type = File_system::Directory_entry::TYPE_DIRECTORY;
				break;
			case Vfs::Directory_service::DIRENT_TYPE_SYMLINK:
				fs_dirent->type = File_system::Directory_entry::TYPE_SYMLINK;
				break;
			case Vfs::Directory_service::DIRENT_TYPE_FILE:
			default:
				fs_dirent->type = File_system::Directory_entry::TYPE_FILE;
				break;
			}
			strncpy(fs_dirent->name, vfs_dirent.name, MAX_NAME_LEN);

			dst += blocksize;
		}

		return out_result;
	}
};

#endif /* _VFS__NODE_H_ */
