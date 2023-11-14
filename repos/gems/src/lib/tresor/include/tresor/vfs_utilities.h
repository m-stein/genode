/*
 * \brief  Utilities for a more convenient use of the VFS
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__VFS_UTILITIES_H_
#define _TRESOR__VFS_UTILITIES_H_

/* base includes */
#include <util/string.h>

/* os includes */
#include <vfs/vfs_handle.h>
#include <vfs/simple_env.h>

/* tresor includes */
#include <tresor/assertion.h>

namespace Tresor {

	using namespace Genode;

	template <typename> class File;
}

Vfs::Vfs_handle &vfs_open(Vfs::Env &, Genode::String<128>, Vfs::Directory_service::Open_mode);

Vfs::Vfs_handle &vfs_open_wo(Vfs::Env &, Genode::String<128>);

Vfs::Vfs_handle &vfs_open_rw(Vfs::Env &, Genode::String<128>);

template <typename STATE>
class Tresor::File
{
	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;

		enum State { IDLE, READ_QUEUED, READ_INITIALIZED, WRITE_INITIALIZED, WRITE_OFFSET_APPLIED };

		State _state { IDLE };
		Vfs::Vfs_handle &_handle;
		Vfs::file_offset _file_offset { 0 };
		Vfs::file_size _file_size { 0 };

	public:

		File(Vfs::Vfs_handle &handle) : _handle { handle } { }

		~File() { ASSERT(_state == IDLE); }

		void read(STATE &caller_state, STATE succeeded, STATE failed, Vfs::file_offset off, Byte_range_ptr dst, bool &progress)
		{
			switch (_state) {
			case IDLE:

				_file_offset = 0;
				_file_size = dst.num_bytes;
				_state = READ_INITIALIZED;
				progress = true;
				break;

			case READ_INITIALIZED:

				_handle.seek(off + _file_offset);
				if (!_handle.fs().queue_read(&_handle, _file_size))
					return;

				_state = READ_QUEUED;
				progress = true;
				break;

			case READ_QUEUED:
			{
				size_t num_read_bytes { 0 };
				switch (_handle.fs().complete_read(&_handle, { dst.start + _file_offset, _file_size }, num_read_bytes)) {
				case Read_result::READ_QUEUED:
				case Read_result::READ_ERR_WOULD_BLOCK: break;
				case Read_result::READ_OK:

					_file_offset += num_read_bytes;
					_file_size -= num_read_bytes;
					if (_file_size) {
						_state = READ_INITIALIZED;
						progress = true;
						break;
					}
					_state = IDLE;
					caller_state = succeeded;
					progress = true;
					break;

				default:

					error("read failed");
					caller_state = failed;
					_state = IDLE;
					progress = true;
					break;
				}
				break;
			}
			default: ASSERT_NEVER_REACHED;
			}
		}

		void write(STATE &caller_state, STATE succeeded, STATE failed, Vfs::file_offset off, Const_byte_range_ptr src, bool &progress)
		{
			switch (_state) {
			case IDLE:

				_file_offset = 0;
				_file_size = src.num_bytes;
				_state = WRITE_INITIALIZED;
				progress = true;
				break;

			case WRITE_INITIALIZED:

				_handle.seek(off + _file_offset);
				_state = WRITE_OFFSET_APPLIED;
				progress = true;
				return;

			case WRITE_OFFSET_APPLIED:
			{
				size_t num_written_bytes { 0 };
				switch (_handle.fs().write(&_handle, { src.start + _file_offset, _file_size }, num_written_bytes)) {
				case Write_result::WRITE_ERR_WOULD_BLOCK: return;
				case Write_result::WRITE_OK:

					_file_offset += num_written_bytes;
					_file_size -= num_written_bytes;
					if (_file_size) {
						_state = WRITE_INITIALIZED;
						progress = true;
						return;
					}
					_state = IDLE;
					caller_state = succeeded;
					progress = true;
					break;

				default:

					error("write failed");
					caller_state = failed;
					_state = IDLE;
					progress = true;
					break;
				}
				break;
			}
			default: ASSERT_NEVER_REACHED;
			}
		}
};

#endif /* _TRESOR__VFS_UTILITIES_H_ */
