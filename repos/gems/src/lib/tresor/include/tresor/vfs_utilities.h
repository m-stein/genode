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

	using Path = String<128>;

	template <typename> class File;
	template <typename> class Read_write_file;
}

Vfs::Vfs_handle &vfs_open(Vfs::Env &, Genode::String<128>, Vfs::Directory_service::Open_mode);

Vfs::Vfs_handle &vfs_open_wo(Vfs::Env &, Genode::String<128>);

Vfs::Vfs_handle &vfs_open_rw(Vfs::Env &, Genode::String<128>);

template <typename HOST_STATE>
class Tresor::File
{
	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using Open_result = Vfs::Directory_service::Open_result;

		enum State { IDLE, READ_QUEUED, READ_INITIALIZED, WRITE_INITIALIZED, WRITE_OFFSET_APPLIED };

		HOST_STATE &_host_state;
		State _state { IDLE };
		Vfs::Vfs_handle &_handle;
		Vfs::file_size _num_processed_bytes { 0 };

		static Vfs::Vfs_handle &_open(Vfs::Env &env, Tresor::Path path, Vfs::Directory_service::Open_mode mode)
		{
			Vfs::Vfs_handle *handle { nullptr };
			Open_result result { env.root_dir().open(path.string(), mode, &handle, env.alloc()) };
			if (result != Open_result::OPEN_OK) {
				error("failed to open file ", path.string());
				class Failed { };
				throw Failed { };
			}
			return *handle;
		}

	public:

		File(HOST_STATE &host_state, Vfs::Vfs_handle &handle) : _host_state { host_state }, _handle { handle } { }

		File(HOST_STATE &host_state, Vfs::Env &env, Tresor::Path path, Vfs::Directory_service::Open_mode mode)
		: _host_state { host_state }, _handle { _open(env, path, mode) } { }

		~File() { ASSERT(_state == IDLE); }

		void read(HOST_STATE succeeded, HOST_STATE failed, Vfs::file_offset off, Byte_range_ptr dst, bool &progress)
		{
			switch (_state) {
			case IDLE:

				_num_processed_bytes = 0;
				_state = READ_INITIALIZED;
				progress = true;
				break;

			case READ_INITIALIZED:

				_handle.seek(off + _num_processed_bytes);
				if (!_handle.fs().queue_read(&_handle, dst.num_bytes - _num_processed_bytes))
					return;

				_state = READ_QUEUED;
				progress = true;
				break;

			case READ_QUEUED:
			{
				size_t num_read_bytes { 0 };
				Byte_range_ptr curr_dst { dst.start + _num_processed_bytes, dst.num_bytes - _num_processed_bytes };
				switch (_handle.fs().complete_read(&_handle, curr_dst, num_read_bytes)) {
				case Read_result::READ_QUEUED:
				case Read_result::READ_ERR_WOULD_BLOCK: break;
				case Read_result::READ_OK:

					_num_processed_bytes += num_read_bytes;
					if (_num_processed_bytes < dst.num_bytes) {
						_state = READ_INITIALIZED;
						progress = true;
						break;
					}
					ASSERT(_num_processed_bytes == dst.num_bytes);
					_state = IDLE;
					_host_state = succeeded;
					progress = true;
					break;

				default:

					error("read failed");
					_host_state = failed;
					_state = IDLE;
					progress = true;
					break;
				}
				break;
			}
			default: ASSERT_NEVER_REACHED;
			}
		}

		void write(HOST_STATE succeeded, HOST_STATE failed, Vfs::file_offset off, Const_byte_range_ptr src, bool &progress)
		{
			switch (_state) {
			case IDLE:

				_num_processed_bytes = 0;
				_state = WRITE_INITIALIZED;
				progress = true;
				break;

			case WRITE_INITIALIZED:

				_handle.seek(off + _num_processed_bytes);
				_state = WRITE_OFFSET_APPLIED;
				progress = true;
				return;

			case WRITE_OFFSET_APPLIED:
			{
				size_t num_written_bytes { 0 };
				Const_byte_range_ptr curr_src { src.start + _num_processed_bytes, src.num_bytes - _num_processed_bytes };
				switch (_handle.fs().write(&_handle, curr_src, num_written_bytes)) {
				case Write_result::WRITE_ERR_WOULD_BLOCK: return;
				case Write_result::WRITE_OK:

					_num_processed_bytes += num_written_bytes;
					if (_num_processed_bytes < src.num_bytes) {
						_state = WRITE_INITIALIZED;
						progress = true;
						break;
					}
					ASSERT(_num_processed_bytes == src.num_bytes);
					_state = IDLE;
					_host_state = succeeded;
					progress = true;
					break;;

				default:

					error("write failed");
					_host_state = failed;
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

template <typename HOST_STATE>
struct Tresor::Read_write_file : public File<HOST_STATE>
{
	Read_write_file(HOST_STATE &host_state, Vfs::Env &env, Tresor::Path path)
	: File<HOST_STATE> { host_state, env, path, Vfs::Directory_service::OPEN_MODE_RDWR } { }
};

#endif /* _TRESOR__VFS_UTILITIES_H_ */
