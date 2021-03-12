/*
 * \brief  Front-end API for accessing a component-local virtual file system
 * \author Norman Feske
 * \date   2017-07-04
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <os/vfs.h>

using namespace Genode;


Genode::File_content::File_content(Allocator &alloc, Directory const &dir, Path const &rel_path,
                                   Limit limit)
:
	_buffer(alloc, min(dir.file_size(rel_path), (Vfs::file_size)limit.value))
{
	if (Readonly_file(dir, rel_path).read(_buffer.ptr, _buffer.size) != _buffer.size) {
		throw Truncated_during_read();
	}
}


size_t Genode::Readonly_file::read(At at, char *dst, size_t bytes) const
{
	Vfs::file_size out_count = 0;

	_handle->seek(at.value);

	while (!_handle->fs().queue_read(_handle, bytes))
		_ep.wait_and_dispatch_one_io_signal();

	Vfs::File_io_service::Read_result result;

	for (;;) {
		result = _handle->fs().complete_read(_handle, dst, bytes,
		                                     out_count);

		if (result != Vfs::File_io_service::READ_QUEUED)
			break;

		_ep.wait_and_dispatch_one_io_signal();
	};

	/*
	 * XXX handle READ_ERR_AGAIN, READ_ERR_WOULD_BLOCK, READ_QUEUED
	 */

	if (result != Vfs::File_io_service::READ_OK) {
		throw Truncated_during_read();
	}
	return out_count;
}
