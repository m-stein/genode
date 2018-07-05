/*
 * \brief  VFS content initialization plugin
 * \author Emery Hemingway
 * \date   2018-07-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <gems/vfs.h>
#include <vfs/print.h>
#include <base/heap.h>

namespace Vfs_import {
	using namespace Vfs;
	class File_system;
	using Genode::Directory;
	using Genode::Root_directory;
}

class Vfs_import::File_system : public Vfs::File_system
{
	private:

		/**
		 * XXX: A would-be temporary heap, but
		 * deconstructing a VFS is not supported.
		 */
		Genode::Heap _heap;

		enum { CREATE_IT = true };

		static void copy_file(Vfs::File_system &dst, Root_directory &src,
		                      Directory::Path const &path,
		                      Genode::Allocator &alloc)
		{
			using Genode::Readonly_file;

			Readonly_file src_file(src, path);
			Vfs_handle *dst_handle = nullptr;

			auto const mode = OPEN_MODE_WRONLY | OPEN_MODE_CREATE;
			auto res = dst.open(path.string(), mode, &dst_handle, alloc);
			if (res != OPEN_OK) {
				Genode::warning("skipping copy of file ", path, ", ", res);
				return;
			}

			{
				char buf[4096];
				Vfs_handle::Guard guard(dst_handle);
				Readonly_file::At at { };

				while (true) {
					file_size wn = 0;
					file_size rn = src_file.read(at, buf, sizeof(buf));
					if (!rn) break;

					auto wres = dst_handle->fs().write(dst_handle, buf, rn, wn);
					switch (wres) {
					case WRITE_OK:
					case WRITE_ERR_AGAIN:
					case WRITE_ERR_WOULD_BLOCK:
						break;
					default:
						Genode::error("failed to write to ", path, ", ", wres);
						dst.unlink(path.string());
						return;
					}

					dst_handle->advance_seek(wn);
					at.value += wn;
				}
			}
		}

		static void copy_dir(Vfs::File_system &dst, Root_directory &src,
		                     Directory::Path const &path,
		                     Genode::Allocator &alloc)
		{
			{
				Vfs_handle *dir_handle = nullptr;
				dst.opendir(path.string(), CREATE_IT, &dir_handle, alloc);
				if (dir_handle)
					dir_handle->close();
			}

			{
				Directory dir(src, path);
				dir.for_each_entry([&] (Directory::Entry const &e) {
					auto entry_path = Directory::join(path, e.name());
					switch (e.type()) {
					case DIRENT_TYPE_FILE:
						copy_file(dst, src, entry_path, alloc);
						break;
					case DIRENT_TYPE_DIRECTORY:
						copy_dir(dst, src, entry_path, alloc);
						break;
					default:
						Genode::warning("skipping copy of ", e);
						break;
					}
				});
			}
		}

	public:

		File_system(Vfs::Env &env, Genode::Xml_node config)
		: _heap(env.env().pd(), env.env().rm())
		{
			Root_directory content(env.env(), _heap, config);
			copy_dir(env.root_dir(), content, Directory::Path(""), _heap);
		}

		const char* type() override { return "init"; }

		/***********************
		 ** Directory service **
		 ***********************/

		Genode::Dataspace_capability dataspace(char const*) override {
			return Genode::Dataspace_capability(); }

		void release(char const*, Dataspace_capability) override { }

		Open_result open(const char*, unsigned, Vfs::Vfs_handle**, Genode::Allocator&) override {
			return Open_result::OPEN_ERR_UNACCESSIBLE; }

		Opendir_result opendir(char const*, bool,
	                           Vfs_handle**, Allocator&) override {
			return OPENDIR_ERR_LOOKUP_FAILED; }

		void close(Vfs::Vfs_handle*) override { }

		Stat_result stat(const char*, Vfs::Directory_service::Stat&) override {
			return STAT_ERR_NO_ENTRY; }

		Unlink_result unlink(const char*) override { return UNLINK_ERR_NO_ENTRY; }

		Rename_result rename(const char*, const char*) override {
			return RENAME_ERR_NO_ENTRY; }

		file_size num_dirent(const char*) override {
			return 0; }

		bool directory(char const*) override {
			return false; }

		const char* leaf_path(const char *) override {
			return nullptr; }

		/**********************
		 ** File I/O service **
		 **********************/

		Write_result write(Vfs_handle*,
		                   const char *, file_size,
		                   file_size &) override {
			return WRITE_ERR_INVALID; }

		Read_result complete_read(Vfs_handle*,
		                          char*, file_size,
		                          file_size&) override {
			return READ_ERR_INVALID; }

		bool read_ready(Vfs_handle*) override {
			return true; }

		bool notify_read_ready(Vfs_handle*) override {
			return false; }

		Ftruncate_result ftruncate(Vfs_handle*, file_size) override {
			return FTRUNCATE_ERR_NO_PERM; }

		Sync_result complete_sync(Vfs_handle*) override {
			return SYNC_OK; }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc())
				Vfs_import::File_system(env, config);
		}
	};

	static Factory f;
	return &f;
}
