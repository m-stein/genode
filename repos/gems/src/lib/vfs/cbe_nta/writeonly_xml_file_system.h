/*
 * \brief  File system for providing a value as a file
 * \author Martin Stein
 * \date   2023-01-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VALUE_FILE_SYSTEM_H_
#define _VALUE_FILE_SYSTEM_H_

/* base includes */
#include <util/xml_generator.h>
#include <base/registry.h>

/* os includes */
#include <vfs/single_file_system.h>

namespace Vfs {

	class Writeonly_xml_file_system;
}


class Vfs::Writeonly_xml_file_system : public Vfs::Single_file_system
{
	public:

		using File_name = Genode::String<64>;
		using Config = Genode::String<200>;

		class Xml_consumer : public Genode::Interface
		{
			public:

				virtual void consume_xml(Genode::Xml_node const &node) = 0;
		};

	private:

		class Vfs_handle : public Single_vfs_handle
		{
			private:

				Writeonly_xml_file_system &_fs;

				Vfs_handle(Vfs_handle const &);

				Vfs_handle &operator = (Vfs_handle const &);

			public:

				Vfs_handle(Writeonly_xml_file_system &fs,
				           Allocator                 &alloc);

				Read_result read(char      *buf_ptr,
				                 file_size  buf_size,
				                 file_size &nr_of_read_bytes) override;

				Write_result write(char const *src,
				                   file_size   count,
				                   file_size  &out_count) override;

				bool read_ready()  const override { return false; }

				bool write_ready() const override { return true; }
		};

		File_name const  _file_name;
		Xml_consumer    &_xml_consumer;

		static char const *_static_type() { return "writeonly_xml"; }

		Config _config(File_name const &file_name) const;

	public:

		Writeonly_xml_file_system(File_name const &file_name,
		                          Xml_consumer    &xml_consumer);

		char const *type() override { return _static_type(); }

		Ftruncate_result ftruncate(Vfs::Vfs_handle *handle,
		                           file_size        size) override;

		Open_result open(char const       *path,
		                 unsigned          mode,
		                 Vfs::Vfs_handle **out_handle,
		                 Allocator        &alloc) override;

		Stat_result stat(char const *path, Stat &out) override;

		Watch_result watch(char const        *path,
		                   Vfs_watch_handle **handle,
		                   Allocator         &alloc) override;

		void close(Vfs_watch_handle * /* handle */) override { }
};

#endif /* _VALUE_FILE_SYSTEM_H_ */
