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

/* tresor includes */
#include <tresor/block_io.h>
#include <tresor/hash.h>

using namespace Tresor;


bool Block_io_sync::execute(Vfs::Vfs_handle &file_handle)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, file_handle);
		_helper.state = SYNC;
		progress = true;
		break;

	case SYNC: _file->sync(SYNC_OK, FILE_ERR, progress); break;
	case SYNC_OK: _helper.mark_succeeded(progress); break;
	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Block_io_read::execute(Vfs::Vfs_handle &file_handle)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, file_handle);
		_helper.state = READ;
		progress = true;
		break;

	case READ: _file->read(READ_OK, FILE_ERR, _helper.attr.in_pba * BLOCK_SIZE, { (char *)&_helper.attr.out_block, BLOCK_SIZE }, progress); break;
	case READ_OK:

		_helper.mark_succeeded(progress);
		if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == _helper.attr.in_pba))
			log("block_io: ", *this, " hash ", hash(_helper.attr.out_block));
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}


bool Block_io_write::execute(Vfs::Vfs_handle &file_handle)
{
	bool progress = false;
	switch (_helper.state) {
	case INIT:

		_file.construct(_helper.state, file_handle);
		_helper.state = WRITE;
		progress = true;
		break;

	case WRITE: _file->write(WRITE_OK, FILE_ERR, _helper.attr.in_pba * BLOCK_SIZE, { (char *)&_helper.attr.in_block, BLOCK_SIZE }, progress); break;
	case WRITE_OK:

		_helper.mark_succeeded(progress);
		if (VERBOSE_BLOCK_IO && (!VERBOSE_BLOCK_IO_PBA_FILTER || VERBOSE_BLOCK_IO_PBA == _helper.attr.in_pba))
			log("block_io: ", *this, " hash ", hash(_helper.attr.in_block));
		break;

	case FILE_ERR: _helper.mark_failed(progress, "file operation failed"); break;
	default: break;
	}
	return progress;
}
