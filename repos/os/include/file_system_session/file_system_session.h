/*
 * \brief  File-system session interface
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__FILE_SYSTEM_SESSION_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__FILE_SYSTEM_SESSION_H_

#include <base/exception.h>
#include <os/packet_stream.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <session/session.h>

namespace File_system {

	struct Node_handle;
	struct File_handle;
	struct Dir_handle;
	struct Symlink_handle;

	using Genode::size_t;

	typedef Genode::uint64_t seek_off_t;
	typedef Genode::uint64_t file_size_t;

	class Packet_descriptor;

	/**
	 * Flags as supplied to 'file', 'dir', and 'symlink' calls
	 */
	enum Mode { STAT_ONLY = 0, READ_ONLY = 1, WRITE_ONLY = 2, READ_WRITE = 3 };

	enum { MAX_NAME_LEN = 256, MAX_PATH_LEN = 1024 };

	typedef Genode::Rpc_in_buffer<MAX_NAME_LEN> Name;
	typedef Genode::Rpc_in_buffer<MAX_PATH_LEN> Path;

	struct Status;
	struct Control;
	struct Directory_entry;

	/*
	 * Exception types
	 */
	class Exception           : public Genode::Exception { };
	class Invalid_handle      : Exception { };
	class Invalid_name        : Exception { };
	class Lookup_failed       : Exception { };
	class Name_too_long       : Exception { };
	class Node_already_exists : Exception { };
	class No_space            : Exception { };
	class Not_empty           : Exception { };
	class Out_of_metadata     : Exception { };
	class Permission_denied   : Exception { };

	struct Session;
}


struct File_system::Node_handle
{
	int value;

	Node_handle() : value(-1) { }
	Node_handle(int v) : value(v) { }

	bool valid() const { return value != -1; }

	bool operator == (Node_handle const &other) const { return other.value == value; }
	bool operator != (Node_handle const &other) const { return other.value != value; }

};


struct File_system::File_handle : Node_handle
{
	File_handle() { }
	File_handle(int v) : Node_handle(v) { }
};


struct File_system::Dir_handle : Node_handle
{
	Dir_handle() { }
	Dir_handle(int v) : Node_handle(v) { }
};


struct File_system::Symlink_handle : Node_handle
{
	Symlink_handle() { }
	Symlink_handle(int v) : Node_handle(v) { }
};


class File_system::Packet_descriptor : public Genode::Packet_descriptor
{
	public:

		enum Opcode { READ, WRITE };

	private:

		Node_handle _handle;   /* node handle */
		Opcode      _op;       /* requested operation */
		seek_off_t  _position; /* file seek offset in bytes */
		size_t      _length;   /* transaction length in bytes */
		bool        _success;  /* indicates success of operation */

	public:

		/**
		 * Constructor
		 */
		Packet_descriptor(Genode::off_t  buf_offset = 0,
		                  Genode::size_t buf_size   = 0)
		:
			Genode::Packet_descriptor(buf_offset, buf_size), _handle(-1),
			_op(READ), _position(0), _length(0), _success(false) { }

		/**
		 * Constructor
		 *
		 * \param position  seek offset in bytes (by default, append)
		 */
		Packet_descriptor(Packet_descriptor p,
		                  Node_handle handle, Opcode op, size_t length,
		                  seek_off_t position = ~0)
		:
			Genode::Packet_descriptor(p.offset(), p.size()),
			_handle(handle), _op(op),
			_position(position), _length(length), _success(false)
		{ }

		Node_handle handle()    const { return _handle;   }
		Opcode      operation() const { return _op;       }
		seek_off_t  position()  const { return _position; }
		size_t      length()    const { return _length;   }
		bool        succeeded() const { return _success;  }

		/*
		 * Accessors called at the server side
		 */
		void succeeded(bool b) { _success = b ? 1 : 0; }
		void length(size_t length) { _length = length; }
};


struct File_system::Status
{
	enum {
		MODE_SYMLINK   = 0020000,
		MODE_FILE      = 0100000,
		MODE_DIRECTORY = 0040000,
	};

	/*
	 * XXX  add access time
	 * XXX  add executable bit
	 */

	file_size_t   size;
	unsigned      mode;
	unsigned long inode;

	bool is_directory() const { return mode & MODE_DIRECTORY; }
	bool is_symlink()   const { return mode & MODE_SYMLINK; }
};


struct File_system::Control { /* to manipulate the executable bit */ };


/**
 * Data structure returned when reading from a directory node
 */
struct File_system::Directory_entry
{
	enum Type { TYPE_FILE, TYPE_DIRECTORY, TYPE_SYMLINK };
	unsigned inode;
	Type     type;
	char     name[MAX_NAME_LEN];
};


struct File_system::Session : public Genode::Session
{
	enum { TX_QUEUE_SIZE = 16 };

	typedef Genode::Packet_stream_policy<File_system::Packet_descriptor,
	                                     TX_QUEUE_SIZE, TX_QUEUE_SIZE,
	                                     char> Tx_policy;

	typedef Packet_stream_tx::Channel<Tx_policy> Tx;

	static const char *service_name() { return "File_system"; }

	virtual ~Session() { }

	/**
	 * Request client-side packet-stream interface of tx channel
	 */
	virtual Tx::Source *tx() { return 0; }

	/**
	 * Open or create file
	 *
	 * \throw Invalid_handle       directory handle is invalid
	 * \throw Invalid_name         file name contains invalid characters
	 * \throw Lookup_failed        the name refers to a node other than a file
	 * \throw Node_already_exists  file cannot be created because a node with
	 *                             the same name already exists
	 * \throw No_space             storage exhausted
	 * \throw Out_of_metadata      server cannot allocate metadata
	 * \throw Permission_denied
	 */
	virtual File_handle file(Dir_handle, Name const &name, Mode, bool create) = 0;

	/**
	 * Open or create symlink
	 *
	 * \throw Invalid_handle       directory handle is invalid
	 * \throw Invalid_name         symlink name contains invalid characters
	 * \throw Lookup_failed        the name refers to a node other than a symlink
	 * \throw Node_already_exists  symlink cannot be created because a node with
	 *                             the same name already exists
	 * \throw No_space             storage exhausted
	 * \throw Out_of_metadata      server cannot allocate metadata
	 * \throw Permission_denied
	 */
	virtual Symlink_handle symlink(Dir_handle, Name const &name, bool create) = 0;

	/**
	 * Open or create directory
	 *
	 * \throw Lookup_failed        path lookup failed because one element
	 *                             of 'path' does not exist
	 * \throw Name_too_long        'path' is too long
	 * \throw Node_already_exists  directory cannot be created because a
	 *                             node with the same name already exists
	 * \throw No_space             storage exhausted
	 * \throw Out_of_metadata      server cannot allocate metadata
	 * \throw Permission_denied
	 */
	virtual Dir_handle dir(Path const &path, bool create) = 0;

	/**
	 * Open existing node
	 *
	 * The returned node handle can be used merely as argument for
	 * 'status'.
	 *
	 * \throw Lookup_failed    path lookup failed because one element
	 *                         of 'path' does not exist
	 * \throw Out_of_metadata  server cannot allocate metadata
	 */
	virtual Node_handle node(Path const &path) = 0;

	/**
	 * Close file
	 */
	virtual void close(Node_handle) = 0;

	/**
	 * Request information about an open file or directory
	 */
	virtual Status status(Node_handle) = 0;

	/**
	 * Set information about an open file or directory
	 */
	virtual void control(Node_handle, Control) = 0;

	/**
	 * Delete file or directory
	 *
	 * \throw Invalid_handle     directory handle is invalid
	 * \throw Not_empty          argument is a non-empty directory and
	 *                           the backend does not support recursion
	 * \throw Permission_denied
	 */
	virtual void unlink(Dir_handle, Name const &) = 0;

	/**
	 * Truncate or grow file to specified size
	 *
	 * \throw Invalid_handle     node handle is invalid
	 * \throw No_space           new size exceeds free space
	 * \throw Permission_denied  node modification not allowed
	 */
	virtual void truncate(File_handle, file_size_t size) = 0;

	/**
	 * Move and rename directory entry
	 *
	 * \throw Invalid_handle     a directory handle is invalid
	 * \throw Invalid_name       'to' contains invalid characters
	 * \throw Lookup_failed      'from' not found
	 * \throw Permission_denied  node modification not allowed
	 */
	virtual void move(Dir_handle, Name const &from,
	                  Dir_handle, Name const &to) = 0;

	/**
	 * Register handler that should be notified on node changes
	 */
	virtual void sigh(Node_handle, Genode::Signal_context_capability sigh) = 0;

	/**
	 * Synchronize file system
	 *
	 * This is only needed by file systems that maintain an internal
	 * cache, which needs to be flushed on certain occasions.
	 */
	virtual void sync(Node_handle) { }


	/*******************
	 ** RPC interface **
	 *******************/

	GENODE_RPC(Rpc_tx_cap, Genode::Capability<Tx>, _tx_cap);
	GENODE_RPC_THROW(Rpc_file, File_handle, file,
	                 GENODE_TYPE_LIST(Invalid_handle, Invalid_name,
	                                  Lookup_failed, Node_already_exists,
	                                  No_space, Out_of_metadata,
	                                  Permission_denied),
	                 Dir_handle, Name const &, Mode, bool);
	GENODE_RPC_THROW(Rpc_symlink, Symlink_handle, symlink,
	                 GENODE_TYPE_LIST(Invalid_handle, Invalid_name,
	                                  Lookup_failed,  Node_already_exists,
	                                  No_space, Out_of_metadata,
	                                  Permission_denied),
	                 Dir_handle, Name const &, bool);
	GENODE_RPC_THROW(Rpc_dir, Dir_handle, dir,
	                 GENODE_TYPE_LIST(Lookup_failed, Name_too_long,
	                                  Node_already_exists, No_space,
	                                  Out_of_metadata, Permission_denied),
	                 Path const &, bool);
	GENODE_RPC_THROW(Rpc_node, Node_handle, node,
	                 GENODE_TYPE_LIST(Lookup_failed, Out_of_metadata),
	                 Path const &);
	GENODE_RPC(Rpc_close, void, close, Node_handle);
	GENODE_RPC(Rpc_status, Status, status, Node_handle);
	GENODE_RPC(Rpc_control, void, control, Node_handle, Control);
	GENODE_RPC_THROW(Rpc_unlink, void, unlink,
	                 GENODE_TYPE_LIST(Invalid_handle, Not_empty,
	                                  Permission_denied),
	                 Dir_handle, Name const &);
	GENODE_RPC_THROW(Rpc_truncate, void, truncate,
	                 GENODE_TYPE_LIST(Invalid_handle, No_space,
	                                  Permission_denied),
	                 File_handle, file_size_t);
	GENODE_RPC_THROW(Rpc_move, void, move,
	                 GENODE_TYPE_LIST(Invalid_handle, Invalid_name,
	                                  Lookup_failed, Permission_denied),
	                 Dir_handle, Name const &, Dir_handle, Name const &);
	GENODE_RPC_THROW(Rpc_sigh, void, sigh,
	                 GENODE_TYPE_LIST(Invalid_handle),
	                 Node_handle, Genode::Signal_context_capability);
	GENODE_RPC(Rpc_sync, void, sync, Node_handle);

	/*
	 * Manual type-list definition, needed because the RPC interface
	 * exceeds the maximum number of type-list elements supported by
	 * 'Genode::Meta::Type_list<>'.
	 */
	typedef Genode::Meta::Type_tuple<Rpc_tx_cap,
	        Genode::Meta::Type_tuple<Rpc_file,
	        Genode::Meta::Type_tuple<Rpc_symlink,
	        Genode::Meta::Type_tuple<Rpc_dir,
	        Genode::Meta::Type_tuple<Rpc_node,
	        Genode::Meta::Type_tuple<Rpc_close,
	        Genode::Meta::Type_tuple<Rpc_status,
	        Genode::Meta::Type_tuple<Rpc_control,
	        Genode::Meta::Type_tuple<Rpc_unlink,
	        Genode::Meta::Type_tuple<Rpc_truncate,
	        Genode::Meta::Type_tuple<Rpc_move,
	        Genode::Meta::Type_tuple<Rpc_sigh,
	        Genode::Meta::Type_tuple<Rpc_sync,
	                         Genode::Meta::Empty>
	        > > > > > > > > > > > > Rpc_functions;
};

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__FILE_SYSTEM_SESSION_H_ */
